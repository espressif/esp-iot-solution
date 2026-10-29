# SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

import os
import sys
import argparse
import shutil
import tempfile
import struct
from io import BytesIO
from PIL import Image, ImageOps
import numpy as np

class AlphaComplexityAnalyzer:
    """Alpha channel complexity analyzer"""

    # Complexity scoring thresholds - adjusted based on ESP32-P4 actual decoding test results
    # Adjusted thresholds are more suitable for UI icon processing
    HIGH_COMPLEXITY_THRESHOLD = 0.25   # Increased threshold to adapt to UI icon edge characteristics
    MEDIUM_COMPLEXITY_THRESHOLD = 0.15  # Increased threshold to reduce false positives for UI icons as medium risk

    def __init__(self):
        self.analysis_cache = {}

    def analyze_alpha_complexity(self, img):
        """Analyze alpha channel complexity of image"""
        if img.mode != 'RGBA':
            img = img.convert('RGBA')

        # Extract alpha channel
        _, _, _, alpha = img.split()
        alpha_array = np.array(alpha)

        # Basic statistics
        unique_values = len(np.unique(alpha_array))
        total_pixels = alpha_array.size

        # UI icon feature detection
        is_likely_ui_icon = self._detect_ui_icon_features(img, alpha_array)

        # Traditional complexity calculation (preserve original logic)
        grad_x = np.abs(np.diff(alpha_array, axis=1))
        grad_y = np.abs(np.diff(alpha_array, axis=0))
        change_intensity = np.mean(grad_x) + np.mean(grad_y)
        unique_ratio = unique_values / total_pixels
        edge_complexity = change_intensity / 255.0

        if is_likely_ui_icon:
            complexity_score = unique_ratio * 0.4 + edge_complexity * 0.6 * 0.7
        else:
            complexity_score = unique_ratio * 0.3 + edge_complexity * 0.7

        # New: Alpha distribution extremeness analysis
        alpha_distribution_analysis = self._analyze_alpha_distribution_extremes(alpha_array)

        # Comprehensive risk assessment: combine traditional complexity and alpha distribution extremeness
        risk_factors = self._calculate_combined_risk(complexity_score, alpha_distribution_analysis)

        return {
            'width': img.width,
            'height': img.height,
            'unique_alpha_values': unique_values,
            'unique_ratio': unique_ratio,
            'edge_complexity': edge_complexity,
            'complexity_score': complexity_score,
            'has_transparency': alpha_array.min() < 255,
            'alpha_min': int(alpha_array.min()),
            'alpha_max': int(alpha_array.max()),
            'alpha_mean': float(alpha_array.mean()),
            'is_likely_ui_icon': is_likely_ui_icon,
            # New fields
            'alpha_distribution': alpha_distribution_analysis,
            'combined_risk_score': risk_factors['combined_risk_score'],
            'risk_factors': risk_factors
        }

    def _detect_ui_icon_features(self, img, alpha_array):
        """Detect if image has UI icon characteristics"""
        # UI icons typically have the following characteristics:
        # 1. Relatively small size (usually smaller than 200x200)
        # 2. Alpha values mainly concentrated at 0 and 255
        # 3. Clear foreground/background separation

        width, height = img.size

        # Feature 1: Size check
        is_small_size = width <= 200 and height <= 200

        # Feature 2: Alpha value distribution check
        alpha_hist, _ = np.histogram(alpha_array, bins=256, range=(0, 256))

        # Check if alpha values are mainly concentrated at 0 and 255
        transparent_pixels = alpha_hist[0]  # Fully transparent
        opaque_pixels = alpha_hist[255]    # Fully opaque
        semi_transparent = np.sum(alpha_hist[1:255])  # Semi-transparent

        total_pixels = alpha_array.size
        transparent_ratio = transparent_pixels / total_pixels
        opaque_ratio = opaque_pixels / total_pixels
        semi_ratio = semi_transparent / total_pixels

        # UI icons usually have many fully transparent and fully opaque pixels, with relatively few semi-transparent pixels
        has_clear_separation = (transparent_ratio + opaque_ratio) > 0.7 and semi_ratio < 0.3

        # Feature 3: Check for clear main shape
        non_transparent_pixels = np.sum(alpha_array > 0)
        shape_density = non_transparent_pixels / total_pixels
        has_clear_shape = 0.1 < shape_density < 0.8  # Neither completely blank nor completely filled

        # Comprehensive judgment
        ui_icon_score = 0
        if is_small_size:
            ui_icon_score += 1
        if has_clear_separation:
            ui_icon_score += 2  # This feature has higher weight
        if has_clear_shape:
            ui_icon_score += 1

        return ui_icon_score >= 2  # Consider as UI icon if 2 or more features are met

    def _analyze_alpha_distribution_extremes(self, alpha_array):
        """Analyze extremeness of alpha distribution"""
        total_pixels = alpha_array.size

        # Calculate pixel ratios for different transparency levels
        fully_transparent = np.sum(alpha_array == 0)  # Fully transparent
        fully_opaque = np.sum(alpha_array == 255)     # Fully opaque
        semi_transparent = total_pixels - fully_transparent - fully_opaque  # Semi-transparent

        transparent_ratio = fully_transparent / total_pixels
        opaque_ratio = fully_opaque / total_pixels
        semi_ratio = semi_transparent / total_pixels

        # Calculate alpha distribution extremeness indicators

        # 1. Extremely biased toward opaque (like mode_blue series: 74% opaque)
        extreme_opaque = opaque_ratio > 0.7 and semi_ratio < 0.15

        # 2. Extremely biased toward semi-transparent (like mode_gray series: 80% semi-transparent)
        extreme_semi = semi_ratio > 0.75 and opaque_ratio < 0.1

        # 3. Too many semi-transparent pixels with many alpha value types (potential compression issues)
        complex_semi = semi_ratio > 0.6 and len(np.unique(alpha_array)) > 120

        # 4. Alpha values too dispersed (may cause compression artifacts)
        alpha_dispersion = len(np.unique(alpha_array)) / total_pixels
        high_dispersion = alpha_dispersion > 0.0001 and len(np.unique(alpha_array)) > 100

        # Calculate extremeness risk score
        extremeness_score = 0
        risk_reasons = []

        if extreme_opaque:
            extremeness_score += 0.8  # High risk
            risk_reasons.append(f'Extremely biased toward opaque({opaque_ratio:.1%})')

        if extreme_semi:
            extremeness_score += 1.0  # Very high risk
            risk_reasons.append(f'Extremely biased toward semi-transparent({semi_ratio:.1%})')

        if complex_semi:
            extremeness_score += 0.6
            risk_reasons.append(f'Complex semi-transparent({semi_ratio:.1%}, {len(np.unique(alpha_array))} alpha types)')

        if high_dispersion:
            extremeness_score += 0.4
            risk_reasons.append(f'Over-dispersed alpha values({len(np.unique(alpha_array))} types)')

        return {
            'transparent_ratio': transparent_ratio,
            'opaque_ratio': opaque_ratio,
            'semi_ratio': semi_ratio,
            'extreme_opaque': extreme_opaque,
            'extreme_semi': extreme_semi,
            'complex_semi': complex_semi,
            'high_dispersion': high_dispersion,
            'extremeness_score': extremeness_score,
            'risk_reasons': risk_reasons
        }

    def _calculate_combined_risk(self, complexity_score, alpha_dist):
        """Calculate comprehensive risk score"""
        # Base complexity weight
        base_risk = complexity_score * 0.3

        # Alpha distribution extremeness weight (more important)
        extremeness_risk = alpha_dist['extremeness_score'] * 0.7

        # Combined risk score
        combined_risk_score = base_risk + extremeness_risk

        # Risk factor description
        risk_factors = []
        risk_factors.append(f'Base complexity: {complexity_score:.3f}')
        risk_factors.append(f"Extremeness score: {alpha_dist['extremeness_score']:.3f}")
        risk_factors.extend(alpha_dist['risk_reasons'])

        return {
            'combined_risk_score': combined_risk_score,
            'risk_factors': risk_factors
        }

    def get_conversion_strategy(self, complexity_analysis):
        """Get conversion strategy based on complexity analysis"""
        # Use new combined risk score instead of traditional complexity score
        combined_risk_score = complexity_analysis['combined_risk_score']
        risk_factors = complexity_analysis['risk_factors']
        alpha_dist = complexity_analysis['alpha_distribution']
        is_ui_icon = complexity_analysis.get('is_likely_ui_icon', False)

        # New thresholds based on alpha distribution extremeness
        # Adjusted based on ESP32-P4 actual decoding test results:
        # - back_auto (0.560): can decode → should convert
        # - mode series (0.857-1.462): cannot decode → should keep PNG
        # - auto series (0.022-0.024): can decode → should convert
        HIGH_RISK_THRESHOLD = 0.850   # Below mode series minimum (0.857)
        MEDIUM_RISK_THRESHOLD = 0.78  # Below main_kedu_auto_btn(0.783), above back_auto(0.560)

        if combined_risk_score > HIGH_RISK_THRESHOLD:
            reason = f'Combined risk score too high ({combined_risk_score:.3f} > {HIGH_RISK_THRESHOLD})'
            if alpha_dist['extreme_opaque']:
                reason += ' [Extremely opaque biased]'
            if alpha_dist['extreme_semi']:
                reason += ' [Extremely semi-transparent biased]'
            if alpha_dist['complex_semi']:
                reason += ' [Complex semi-transparent]'
            if is_ui_icon:
                reason += ' [UI icon]'

            return {
                'action': 'copy_as_png',
                'reason': reason,
                'recommendation': 'Copy as PNG format to avoid ESP32-P4 hardware decoding issues',
                'risk_level': 'HIGH',
                'suggested_alpha_quality': None,
                'risk_details': risk_factors
            }

        elif combined_risk_score > MEDIUM_RISK_THRESHOLD:
            # Medium risk, reduce alpha quality
            # Adjust quality based on specific risk factors
            if alpha_dist['extreme_opaque']:
                suggested_quality = 70  # Extremely opaque needs higher quality to maintain edges
            elif alpha_dist['complex_semi']:
                suggested_quality = 60  # Complex semi-transparent reduce quality to minimize artifacts
            else:
                suggested_quality = 75  # Default medium quality

            reason = f'Combined risk score medium ({combined_risk_score:.3f})'
            if is_ui_icon:
                reason += ' [UI icon optimization]'
                suggested_quality = max(70, suggested_quality)  # UI icons maintain higher quality

            return {
                'action': 'convert_with_adjustment',
                'reason': reason,
                'recommendation': f'Reduce alpha quality to {suggested_quality} to optimize decoding performance',
                'risk_level': 'MEDIUM',
                'suggested_alpha_quality': suggested_quality,
                'risk_details': risk_factors
            }
        else:
            reason = f'Combined risk score low ({combined_risk_score:.3f})'
            if is_ui_icon:
                reason += ' [UI icon safe conversion]'
            return {
                'action': 'convert',
                'reason': reason,
                'recommendation': 'Safe conversion to PJPG, enjoy hardware acceleration',
                'risk_level': 'LOW',
                'suggested_alpha_quality': None,
                'risk_details': risk_factors
            }

class PNGAligner:
    def __init__(self, pad_mode='transparent', pad_color=(0, 0, 0, 0)):
        self.pad_mode = pad_mode
        self.pad_color = pad_color

    def check_alignment(self, width, height):
        """Check if dimensions are 16-pixel aligned"""
        return width % 16 == 0 and height % 16 == 0

    def calculate_aligned_size(self, width, height):
        """Calculate 16-pixel aligned dimensions"""
        aligned_width = ((width + 15) // 16) * 16
        aligned_height = ((height + 15) // 16) * 16
        return aligned_width, aligned_height

    def align_image(self, img):
        """Align image to 16-pixel boundaries"""
        original_width, original_height = img.size

        # Check if already aligned
        if self.check_alignment(original_width, original_height):
            print(f'✓ Already aligned: {original_width}x{original_height}')
            return img, False

        # Calculate aligned dimensions
        aligned_width, aligned_height = self.calculate_aligned_size(original_width, original_height)
        pad_width = aligned_width - original_width
        pad_height = aligned_height - original_height

        print(f'→ Aligning: {original_width}x{original_height} → {aligned_width}x{aligned_height}')
        print(f'  Padding: +{pad_width}w, +{pad_height}h')

        x_offset = 0
        y_offset = 0

        # Ensure input image is RGBA
        if img.mode != 'RGBA':
            img = img.convert('RGBA')

        # Create aligned image based on padding mode
        if self.pad_mode == 'transparent':
            # Transparent padding
            aligned_img = Image.new('RGBA', (aligned_width, aligned_height), (0, 0, 0, 0))
            aligned_img.paste(img, (x_offset, y_offset), img)

        elif self.pad_mode == 'edge':
            # Edge extension padding
            aligned_img = ImageOps.pad(img, (aligned_width, aligned_height), method=Image.LANCZOS)
            if aligned_img.mode != 'RGBA':
                aligned_img = aligned_img.convert('RGBA')

        elif self.pad_mode == 'color':
            # Solid color padding
            aligned_img = Image.new('RGBA', (aligned_width, aligned_height), self.pad_color)
            aligned_img.paste(img, (x_offset, y_offset), img)

        else:
            # Default transparent padding
            aligned_img = Image.new('RGBA', (aligned_width, aligned_height), (0, 0, 0, 0))
            aligned_img.paste(img, (x_offset, y_offset), img)

        return aligned_img, True

    def process_image(self, input_file, output_file=None, preserve_name=False, verbose=True):
        """Process a single PNG image"""
        try:
            # Read PNG image
            img = Image.open(input_file)
            original_size = img.size

            # Align image
            aligned_img, was_padded = self.align_image(img)

            # Generate output filename (if not specified)
            if output_file is None:
                if preserve_name:
                    output_file = input_file  # Keep original name
                else:
                    name, ext = os.path.splitext(input_file)
                    output_file = f'{name}_aligned{ext}'

            # Save aligned image
            aligned_img.save(output_file, 'PNG', optimize=True)

            # Print statistics
            if verbose:
                original_file_size = os.path.getsize(input_file)
                aligned_file_size = os.path.getsize(output_file)

                print(f'✓ Saved: {output_file}')
                print(f'  Original: {original_size[0]}x{original_size[1]} ({original_file_size:,} bytes)')
                print(f'  Aligned: {aligned_img.width}x{aligned_img.height} ({aligned_file_size:,} bytes)')
                print(f"  16-aligned: {'Yes' if self.check_alignment(aligned_img.width, aligned_img.height) else 'No'}")
                print(f'  Padding mode: {self.pad_mode}')
                if was_padded:
                    print(f'  Size change: {((aligned_file_size - original_file_size) / original_file_size * 100):+.1f}%')
                print('-' * 50)

            return True, aligned_img

        except Exception as e:
            print(f'✗ Processing failed {input_file}: {str(e)}')
            return False, None

class PJPGConverter:
    def __init__(self, jpeg_quality=85, alpha_quality=85, enable_complexity_check=True):
        self.jpeg_quality = jpeg_quality
        self.alpha_quality = alpha_quality
        self.magic = b'_PJPG__'
        self.version = 1
        self.enable_complexity_check = enable_complexity_check
        self.analyzer = AlphaComplexityAnalyzer() if enable_complexity_check else None

        # Conversion statistics
        self.conversion_stats = {
            'total': 0,
            'converted': 0,
            'copied_as_png': 0,
            'converted_with_adjustment': 0,
            'failed': 0
        }

        # Record PNG files generated by complexity detection, these files should be preserved during cleanup
        self.complexity_generated_pngs = set()

    def convert_png_to_pjpg(self, input_file, output_file=None, input_img=None, verbose=True):
        """Convert PNG file to PJPG format with complexity detection"""
        try:
            self.conversion_stats['total'] += 1

            # Read PNG image or use provided image object
            if input_img is not None:
                img = input_img
            else:
                img = Image.open(input_file)

            if img.mode != 'RGBA':
                img = img.convert('RGBA')
            aligner = PNGAligner('transparent', (0, 0, 0, 0))
            img, _ = aligner.align_image(img)

            # Complexity detection
            if self.enable_complexity_check and self.analyzer:
                complexity_analysis = self.analyzer.analyze_alpha_complexity(img)
                strategy = self.analyzer.get_conversion_strategy(complexity_analysis)

                if verbose:
                    print(f'🔍 Alpha complexity analysis:')
                    print(f"  Complexity score: {complexity_analysis['complexity_score']:.6f}")
                    print(f"  Unique alpha values: {complexity_analysis['unique_alpha_values']}")
                    print(f"  Edge complexity: {complexity_analysis['edge_complexity']:.4f}")
                    print(f"  Risk level: {strategy['risk_level']}")
                    print(f"  Strategy: {strategy['reason']}")

                # Decide processing method based on strategy
                if strategy['action'] == 'copy_as_png':
                    # High complexity files copy as PNG
                    if output_file is None:
                        if input_file:
                            output_file = os.path.splitext(input_file)[0] + '.png'
                        else:
                            output_file = 'output.png'

                    # Ensure output file extension is .png
                    if not output_file.lower().endswith('.png'):
                        output_file = os.path.splitext(output_file)[0] + '.png'

                    # Copy PNG file
                    if input_file and input_file != output_file:
                        import shutil
                        shutil.copy2(input_file, output_file)
                    elif input_img is not None:
                        # If from memory image, save as PNG
                        img.save(output_file, 'PNG', optimize=True)

                    # Record this PNG file as generated by complexity detection, should be preserved during cleanup
                    self.complexity_generated_pngs.add(os.path.abspath(output_file))

                    self.conversion_stats['copied_as_png'] += 1

                    if verbose:
                        original_size = os.path.getsize(input_file) if input_file else 0
                        copied_size = os.path.getsize(output_file)

                        print(f"📋 Copied as PNG: {input_file or 'memory image'} -> {output_file}")
                        print(f'💡 Suggestion: Keep PNG format to avoid ESP32-P4 hardware decoding timeout')
                        print(f'Image size: {img.width}x{img.height}')
                        if input_file:
                            print(f'File size: {copied_size:,} bytes')
                        print(f"Complexity score: {complexity_analysis['complexity_score']:.6f}")
                        print('-' * 50)

                    return True

                elif strategy['action'] == 'convert_with_adjustment':
                    # Adjust alpha quality
                    original_alpha_quality = self.alpha_quality
                    self.alpha_quality = strategy['suggested_alpha_quality']
                    self.conversion_stats['converted_with_adjustment'] += 1
                    if verbose:
                        print(f'⚙️  Adjusting parameters: Alpha quality {original_alpha_quality} → {self.alpha_quality}')

            # Separate RGB and Alpha channels
            rgb = img.convert('RGB')
            _, _, _, alpha = img.split()
            alpha = alpha.convert('L')

            # Compress RGB data as JPEG
            jpeg_buffer = BytesIO()
            rgb.save(jpeg_buffer, format='JPEG', quality=self.jpeg_quality, subsampling=2, progressive=False, optimize=False)
            jpeg_data = jpeg_buffer.getvalue()

            # Compress Alpha channel as JPEG (grayscale)
            alpha_buffer = BytesIO()
            alpha.save(alpha_buffer, format='JPEG', quality=self.alpha_quality, subsampling=0, progressive=False, optimize=False)
            alpha_jpeg_data = alpha_buffer.getvalue()

            # Prepare file header
            header = struct.pack('<7sB2H2I2x',
                               self.magic,                # Magic number
                               self.version,              # Version
                               img.width,                 # Width
                               img.height,                # Height
                               len(jpeg_data),           # RGB JPEG data length
                               len(alpha_jpeg_data))      # Alpha JPEG data length

            # Generate output filename (if not specified)
            if output_file is None:
                if input_file:
                    output_file = os.path.splitext(input_file)[0] + '.pjpg'
                else:
                    output_file = 'output.pjpg'

            # Ensure output file extension is .pjpg
            if not output_file.lower().endswith('.pjpg'):
                output_file = os.path.splitext(output_file)[0] + '.pjpg'

            # Write file
            with open(output_file, 'wb') as f:
                f.write(header)
                f.write(jpeg_data)
                f.write(alpha_jpeg_data)

            self.conversion_stats['converted'] += 1

            # Print statistics
            if verbose:
                if input_file:
                    original_size = os.path.getsize(input_file)
                else:
                    original_size = 0  # Cannot get original size

                pjpg_size = os.path.getsize(output_file)

                print(f"✅ Conversion completed: {input_file or 'memory image'} -> {output_file}")
                print(f'Image size: {img.width}x{img.height}')
                print(f'JPEG quality - RGB: {self.jpeg_quality}, Alpha: {self.alpha_quality}')
                if input_file:
                    print(f'Original size: {original_size:,} bytes')
                    compression_ratio = pjpg_size / original_size if original_size > 0 else 0
                    print(f'Compression ratio: {compression_ratio:.2%}')
                print(f'PJPG size: {pjpg_size:,} bytes')
                print(f'RGB JPEG size: {len(jpeg_data):,} bytes')
                print(f'Alpha JPEG size: {len(alpha_jpeg_data):,} bytes')
                print('-' * 50)

            return True

        except Exception as e:
            print(f'❌ Conversion failed: {str(e)}')
            self.conversion_stats['failed'] += 1
            return False

    def print_conversion_summary(self):
        """Print conversion summary statistics"""
        stats = self.conversion_stats
        print('\n' + '=' * 60)
        print('🚀 Conversion Summary Statistics')
        print('=' * 60)
        print(f"📊 Total processed: {stats['total']}")
        print(f"✅ Successfully converted: {stats['converted']}")
        print(f"📋 Copied as PNG: {stats['copied_as_png']}")
        print(f"⚙️  Parameter adjusted conversions: {stats['converted_with_adjustment']}")
        print(f"❌ Conversion failed: {stats['failed']}")

        if stats['total'] > 0:
            success_rate = (stats['converted'] + stats['converted_with_adjustment']) / stats['total'] * 100
            print(f'📈 Success rate: {success_rate:.1f}%')

            if stats['copied_as_png'] > 0:
                print(f"⚠️  PNG copy rate: {stats['copied_as_png'] / stats['total'] * 100:.1f}%")

        if stats['copied_as_png'] > 0:
            print(f"\n💡 Suggestion: {stats['copied_as_png']} high complexity files copied as PNG")
            print(f'   These files are recommended to keep as PNG format to avoid ESP32-P4 hardware decoding timeout')

class PNGProcessor:
    def __init__(self, pad_mode='transparent', pad_color=(0, 0, 0, 0),
                 jpeg_quality=85, alpha_quality=85, enable_complexity_check=True):
        self.aligner = PNGAligner(pad_mode, pad_color)
        self.converter = PJPGConverter(jpeg_quality, alpha_quality, enable_complexity_check)
        self.enable_complexity_check = enable_complexity_check

    def process_align_only(self, input_path, output_path, recursive=False, preserve_names=True):
        """Execute 16-pixel alignment only"""
        print('Starting PNG 16-pixel alignment...')
        print('=' * 50)

        if os.path.isfile(input_path):
            # Single file processing
            success, _ = self.aligner.process_image(input_path, output_path, preserve_names)
            return success

        elif os.path.isdir(input_path):
            # Batch processing
            return self._batch_align(input_path, output_path, recursive, preserve_names)

        return False

    def process_convert_only(self, input_path, output_path, recursive=False):
        """Execute PJPG conversion only, with complexity detection"""
        print('Starting PNG to PJPG conversion (with complexity detection)...')
        print('=' * 50)

        if self.enable_complexity_check:
            print('🧠 Alpha complexity intelligent detection enabled')
            print('  - High complexity files will be skipped')
            print('  - Medium complexity files will have adjusted compression parameters')
            print('  - Low complexity files will be converted normally')
            print('-' * 50)

        if os.path.isfile(input_path):
            # Single file processing
            return self.converter.convert_png_to_pjpg(input_path, output_path)

        elif os.path.isdir(input_path):
            # Batch processing
            success = self._batch_convert(input_path, output_path, recursive)
            # Print conversion summary
            self.converter.print_conversion_summary()
            return success

        return False

    def process_combined(self, input_path, output_path, recursive=False, preserve_names=True, temp_dir=None, cleanup_png=False):
        """Combined processing: strategy analysis first, then alignment, finally conversion based on strategy"""
        print('Starting combined processing: Strategy analysis + PNG alignment + PJPG conversion (intelligent detection)...')
        print('=' * 60)

        if self.enable_complexity_check:
            print('🧠 Alpha complexity intelligent detection enabled')
            print('  - Analyze transparency complexity of each PNG file')
            print('  - Automatically skip files that may cause decoding timeout')
            print('  - Intelligently adjust compression parameters to optimize performance')
            print('-' * 60)

        # Pre-analysis step: analyze complexity strategy of original PNG files
        strategies = {}
        if self.enable_complexity_check and os.path.isdir(input_path):
            print('Step 0: Complexity pre-analysis (based on original PNG)')
            print('-' * 30)

            for root, dirs, files in os.walk(input_path):
                if not recursive and root != input_path:
                    continue

                for file in files:
                    if file.lower().endswith('.png'):
                        file_path = os.path.join(root, file)
                        try:
                            img = Image.open(file_path)
                            # Use original image for complexity analysis
                            complexity_analysis = self.converter.analyzer.analyze_alpha_complexity(img)
                            strategy = self.converter.analyzer.get_conversion_strategy(complexity_analysis)
                            strategies[file] = strategy
                            print(f"📄 {file}: {strategy['risk_level']} (combined risk: {complexity_analysis['combined_risk_score']:.3f})")
                        except Exception as e:
                            print(f'❌ Pre-analysis failed {file}: {str(e)}')

            high_risk_count = sum(1 for s in strategies.values() if s['risk_level'] == 'HIGH')
            medium_risk_count = sum(1 for s in strategies.values() if s['risk_level'] == 'MEDIUM')
            low_risk_count = sum(1 for s in strategies.values() if s['risk_level'] == 'LOW')

            print(f'\n📊 Pre-analysis results:')
            print(f'   ❌ High risk: {high_risk_count} files (will keep PNG format)')
            print(f'   ⚠️  Medium risk: {medium_risk_count} files (will convert with quality adjustment)')
            print(f'   ✅ Low risk: {low_risk_count} files (will convert normally)')

        # Create temporary directory for intermediate results
        if temp_dir is None:
            temp_dir = tempfile.mkdtemp(prefix='png_processor_')

        try:
            # Step 1: 16-pixel alignment
            print(f'\nStep 1: PNG 16-pixel alignment')
            print('-' * 30)
            align_success = self.process_align_only(input_path, temp_dir, recursive, preserve_names)

            if not align_success:
                print('Alignment step failed, terminating processing')
                return False

            print(f'\n✅ Alignment completed, intermediate results saved in: {temp_dir}')

            # Step 2: PJPG conversion based on pre-analysis strategy
            print(f'\nStep 2: PJPG conversion (based on pre-analysis strategy)')
            print('-' * 30)
            convert_success = self._batch_convert_with_strategy(temp_dir, output_path, recursive, strategies)

            if not convert_success:
                print('\n❌ PJPG conversion step failed')
                return False

            # Step 3: Clean up PNG files (if needed)
            if cleanup_png:
                print(f'\nStep 3: Clean up PNG files')
                print('-' * 30)
                self._cleanup_png_files(output_path)

            print(f'\n✅ Combined processing completed! Final results saved in: {output_path}')

            return True

        finally:
            # Clean up temporary directory
            if os.path.exists(temp_dir):
                shutil.rmtree(temp_dir)
                print(f'🧹 Cleaned up temporary directory: {temp_dir}')

    def _batch_convert_with_strategy(self, input_dir, output_dir, recursive, strategies):
        """Batch convert based on pre-analysis strategy"""
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        converted_count = 0
        copied_count = 0
        adjusted_count = 0
        total_count = 0

        print(f'Converting PNG files from: {input_dir}')
        print(f'Output directory: {output_dir}')
        print(f'Recursive processing: {recursive}')
        print('=' * 50)

        # Traverse input directory
        for root, dirs, files in os.walk(input_dir):
            if not recursive and root != input_dir:
                continue

            for file in files:
                if file.lower().endswith('.png'):
                    total_count += 1
                    input_path = os.path.join(root, file)

                    # Get pre-analysis strategy
                    strategy = strategies.get(file, {'action': 'convert', 'risk_level': 'LOW'})

                    print(f'[{total_count}] {file}')
                    print(f"   Strategy: {strategy.get('reason', 'DEFAULT')}")

                    if strategy['action'] == 'copy_as_png':
                        # High risk file: Copy as PNG
                        output_filename = os.path.splitext(file)[0] + '.png'
                        output_path = os.path.join(output_dir, output_filename)

                        import shutil
                        shutil.copy2(input_path, output_path)
                        self.converter.complexity_generated_pngs.add(os.path.abspath(output_path))
                        copied_count += 1

                        print(f'📋 Copied as PNG: {output_path}')
                        print(f'💡 Suggestion: Keep PNG format to avoid ESP32-P4 hardware decoding timeout')

                    else:
                        # Medium/low risk file: Convert to PJPG
                        output_filename = os.path.splitext(file)[0] + '.pjpg'
                        output_path = os.path.join(output_dir, output_filename)

                        # Ensure output directory exists
                        os.makedirs(os.path.dirname(output_path), exist_ok=True)

                        # Apply strategy to adjust quality
                        if strategy['action'] == 'convert_with_adjustment':
                            original_alpha_quality = self.converter.alpha_quality
                            self.converter.alpha_quality = strategy['suggested_alpha_quality']
                            print(f'⚙️  Adjust parameters: Alpha quality {original_alpha_quality} → {self.converter.alpha_quality}')
                            adjusted_count += 1

                        # Convert to PJPG (Skip complexity detection, as already pre-analyzed)
                        self.converter.enable_complexity_check = False
                        success = self.converter.convert_png_to_pjpg(input_path, output_path, verbose=False)
                        self.converter.enable_complexity_check = True

                        if success:
                            converted_count += 1
                            print(f'✅ Conversion completed: {output_path}')
                        else:
                            print(f'❌ Conversion failed: {file}')

                    print('-' * 50)

        # Update conversion statistics
        self.converter.conversion_stats['total'] = total_count
        self.converter.conversion_stats['converted'] = converted_count
        self.converter.conversion_stats['copied_as_png'] = copied_count
        self.converter.conversion_stats['converted_with_adjustment'] = adjusted_count

        print(f'\nBatch conversion completed:')
        print(f'   ✅ Converted to PJPG: {converted_count} files')
        print(f'   📋 Copied as PNG: {copied_count} files')
        print(f'   ⚙️  Quality adjustment: {adjusted_count} files')

        # Print conversion summary
        self.converter.print_conversion_summary()

        return total_count > 0

    def analyze_directory_complexity(self, input_dir, recursive=False):
        """Analyze alpha complexity of PNG files in directory (align first then analyze)"""
        if not self.enable_complexity_check:
            print('⚠️  Alpha complexity detection not enabled')
            return

        if not self.converter.analyzer:
            print('❌ Alpha complexity analyzer not initialized')
            return

        analyzer = self.converter.analyzer
        analysis_results = []

        print(f'🔍 Starting to analyze alpha complexity of PNG files in directory...')
        print('=' * 60)

        # Traverse PNG files
        for root, dirs, files in os.walk(input_dir):
            if not recursive and root != input_dir:
                continue

            for file in files:
                if file.lower().endswith('.png'):
                    file_path = os.path.join(root, file)
                    try:
                        img = Image.open(file_path)

                        # First align to 16-pixel boundaries (consistent with conversion stage)
                        aligned_img, was_padded = self.aligner.align_image(img)

                        # Analyze complexity of aligned image
                        complexity_analysis = analyzer.analyze_alpha_complexity(aligned_img)
                        strategy = analyzer.get_conversion_strategy(complexity_analysis)

                        result = {
                            'filename': file,
                            'path': file_path,
                            'complexity': complexity_analysis,
                            'strategy': strategy,
                            'was_padded': was_padded,
                            'original_size': img.size,
                            'aligned_size': aligned_img.size
                        }
                        analysis_results.append(result)

                    except Exception as e:
                        print(f'❌ Analysis failed {file}: {str(e)}')

        # Classification statistics - Use strategy risk level instead of traditional complexity
        high_risk = [r for r in analysis_results if r['strategy']['risk_level'] == 'HIGH']
        medium_risk = [r for r in analysis_results if r['strategy']['risk_level'] == 'MEDIUM']
        low_risk = [r for r in analysis_results if r['strategy']['risk_level'] == 'LOW']

        # Print detailed report
        print(f'\n📊 Complexity analysis report ({len(analysis_results)} files)')
        print('=' * 60)

        print(f'\n❌ High risk files ({len(high_risk)} files) - Copy as PNG format:')
        for r in high_risk:
            c = r['complexity']
            combined_score = c['combined_risk_score']
            print(f"  📄 {r['filename']}")
            print(f"     Combined risk: {combined_score:.3f} | Alpha values: {c['unique_alpha_values']} | Traditional complexity: {c['complexity_score']:.3f}")

        print(f'\n⚠️  Medium risk files ({len(medium_risk)} files) - Convert with reduced Alpha quality:')
        for r in medium_risk:
            c = r['complexity']
            combined_score = c['combined_risk_score']
            suggested_q = r['strategy']['suggested_alpha_quality']
            print(f"  📄 {r['filename']}")
            print(f'     Combined risk: {combined_score:.3f} | Suggested Alpha quality: {suggested_q}')

        print(f'\n✅ Low risk files ({len(low_risk)} files) - Safe conversion to PJPG:')
        for r in low_risk:
            c = r['complexity']
            combined_score = c['combined_risk_score']
            print(f"  📄 {r['filename']} (Combined risk: {combined_score:.3f})")

        # Print summary suggestions
        print(f'\n💡 Processing suggestions:')
        print('=' * 60)
        if len(high_risk) > 0:
            print(f'1. {len(high_risk)} high complexity files will be copied as PNG format')
            print('   These files should keep PNG format to avoid ESP32-P4 hardware decoding timeout')

        if len(medium_risk) > 0:
            print(f'2. {len(medium_risk)} medium complexity files can be converted, but will automatically reduce Alpha quality')
            print('   To optimize decoding performance')

        if len(low_risk) > 0:
            print(f'3. {len(low_risk)} low complexity files can be safely converted to PJPG')
            print('   Enjoy hardware accelerated decoding performance')

        # Return analysis results for subsequent use
        return {
            'total': len(analysis_results),
            'high_risk': high_risk,
            'medium_risk': medium_risk,
            'low_risk': low_risk,
            'all_results': analysis_results
        }

    def _cleanup_png_files(self, output_dir):
        """Clean up PNG files in output directory, but retain PNG files generated by complexity detection"""
        cleaned_count = 0
        preserved_count = 0
        png_files = []

        # Traverse directory to find PNG files
        for root, dirs, files in os.walk(output_dir):
            for file in files:
                if file.lower().endswith('.png'):
                    png_files.append(os.path.join(root, file))

        print(f'Found {len(png_files)} PNG files to check')

        # Get list of PNG files generated by complexity detection
        complexity_pngs = self.converter.complexity_generated_pngs

        # Delete PNG files, but retain complexity detection generated files
        for png_file in png_files:
            abs_png_path = os.path.abspath(png_file)

            if abs_png_path in complexity_pngs:
                # This is a PNG file generated by complexity detection, should be retained
                preserved_count += 1
                print(f'🛡️   Retained complexity PNG: {os.path.basename(png_file)}')
            else:
                # This is an intermediate PNG file, can be deleted
                try:
                    os.remove(png_file)
                    print(f'🗑️   Deleted intermediate PNG: {os.path.basename(png_file)}')
                    cleaned_count += 1
                except Exception as e:
                    print(f'❌ Deletion failed {os.path.basename(png_file)}: {str(e)}')

        print(f'Cleaning completed: Deleted {cleaned_count} intermediate PNG files, retained {preserved_count} complexity PNG files')

        # Statistics final results
        pjpg_count = len([f for f in os.listdir(output_dir) if f.lower().endswith('.pjpg')])
        remaining_png = len([f for f in os.listdir(output_dir) if f.lower().endswith('.png')])

        print(f'Final results: PJPG files {pjpg_count}, PNG files {remaining_png}')

        if remaining_png > 0:
            print(f'💡 Note: {remaining_png} PNG files are high complexity files, retained to avoid decoding timeout')

        if cleaned_count == 0 and preserved_count == 0:
            print('✅ No intermediate PNG files need cleaning')
        else:
            print(f'✅ Cleaning strategy: Delete {cleaned_count} intermediate files, retain {preserved_count} complexity files')

    def _batch_align(self, input_dir, output_dir, recursive, preserve_names):
        """Batch alignment processing"""
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        processed_count = 0
        total_count = 0
        alignment_stats = {'already_aligned': 0, 'padded': 0, 'failed': 0}

        print(f'Processing PNG files from: {input_dir}')
        print(f'Output directory: {output_dir}')
        print(f'Padding mode: {self.aligner.pad_mode}')
        print(f'Keep name: {preserve_names}')
        print(f'Recursive processing: {recursive}')
        print('=' * 50)

        # Traverse input directory
        for root, dirs, files in os.walk(input_dir):
            if not recursive and root != input_dir:
                continue

            for file in files:
                if file.lower().endswith('.png'):
                    total_count += 1
                    input_path = os.path.join(root, file)

                    # Check original alignment status (for statistics)
                    try:
                        with Image.open(input_path) as img:
                            if self.aligner.check_alignment(img.width, img.height):
                                alignment_stats['already_aligned'] += 1
                            else:
                                alignment_stats['padded'] += 1
                    except:
                        alignment_stats['failed'] += 1
                        continue

                    # Build output path
                    rel_path = os.path.relpath(input_path, input_dir)
                    if preserve_names:
                        # Keep original file name
                        output_filename = rel_path
                    else:
                        # Add "_aligned" suffix
                        output_filename = os.path.splitext(rel_path)[0] + '_aligned.png'

                    output_path = os.path.join(output_dir, output_filename)

                    # Ensure output directory exists
                    os.makedirs(os.path.dirname(output_path), exist_ok=True)

                    print(f'[{processed_count + 1}/{total_count}] {file}')
                    success, _ = self.aligner.process_image(input_path, output_path, preserve_names)
                    if success:
                        processed_count += 1
                    else:
                        alignment_stats['failed'] += 1

        # Print summary
        print('\n' + '=' * 50)
        print('Batch alignment processing summary')
        print('=' * 50)
        print(f'Total processed files: {processed_count}/{total_count}')
        print(f"Already aligned: {alignment_stats['already_aligned']} files")
        print(f"Aligned padding: {alignment_stats['padded']} files")
        print(f"Failed: {alignment_stats['failed']} files")
        print(f'Success rate: {(processed_count/total_count*100):.1f}%' if total_count > 0 else 'N/A')

        return processed_count > 0

    def _batch_convert(self, input_dir, output_dir, recursive):
        """Batch conversion processing"""
        if not os.path.exists(output_dir):
            os.makedirs(output_dir)

        converted_count = 0
        total_count = 0

        print(f'Converting PNG files from: {input_dir}')
        print(f'Output directory: {output_dir}')
        print(f'Recursive processing: {recursive}')
        print('=' * 50)

        # Traverse input directory
        for root, dirs, files in os.walk(input_dir):
            if not recursive and root != input_dir:
                continue

            for file in files:
                if file.lower().endswith('.png'):
                    total_count += 1
                    input_path = os.path.join(root, file)

                    # Build output path
                    rel_path = os.path.relpath(input_path, input_dir)
                    output_filename = os.path.splitext(rel_path)[0] + '.pjpg'
                    output_path = os.path.join(output_dir, output_filename)

                    # Ensure output directory exists
                    os.makedirs(os.path.dirname(output_path), exist_ok=True)

                    print(f'[{converted_count + 1}/{total_count}] {file}')
                    if self.converter.convert_png_to_pjpg(input_path, output_path):
                        converted_count += 1

        print(f'\nBatch conversion completed: {converted_count}/{total_count} files successfully converted')
        return converted_count > 0

def parse_color(color_str):
    """Parse color string to RGBA tuple"""
    if color_str.lower() == 'transparent':
        return (0, 0, 0, 0)
    elif color_str.lower() == 'white':
        return (255, 255, 255, 255)
    elif color_str.lower() == 'black':
        return (0, 0, 0, 255)
    elif color_str.startswith('#'):
        # Hex color
        hex_color = color_str[1:]
        if len(hex_color) == 6:
            r = int(hex_color[0:2], 16)
            g = int(hex_color[2:4], 16)
            b = int(hex_color[4:6], 16)
            return (r, g, b, 255)
        elif len(hex_color) == 8:
            r = int(hex_color[0:2], 16)
            g = int(hex_color[2:4], 16)
            b = int(hex_color[4:6], 16)
            a = int(hex_color[6:8], 16)
            return (r, g, b, a)

    # Default transparent
    return (0, 0, 0, 0)

def main():
    parser = argparse.ArgumentParser(
        description='PNG Comprehensive Processing Tool: 16-pixel alignment + PJPG conversion (with Alpha complexity intelligent detection)',
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Usage examples:
  # Combined processing (recommended): align + intelligent convert
  python3 png_processor.py resource_png -o resource --combined

  # Combined processing, keep only PJPG files, disable complexity detection
  python3 png_processor.py resource_png -o resource --combined --pjpg-only --no-complexity-check

  # Only analyze complexity, no conversion
  python3 png_processor.py resource_png --analyze-only

  # Only 16-pixel alignment
  python3 png_processor.py resource_png -o resource --align-only

  # Only PJPG conversion (with intelligent detection)
  python3 png_processor.py resource_png -o resource --convert-only

  # Force conversion (ignore complexity detection)
  python3 png_processor.py resource_png -o resource --convert-only --no-complexity-check
        """
    )

    parser.add_argument('input', help='Input PNG file or directory path')
    parser.add_argument('-o', '--output', help='Output file or directory path (optional for analysis mode)')
    parser.add_argument('-r', '--recursive', action='store_true',
                       help='Recursively process PNG files in subdirectories')

    # Processing mode selection
    mode_group = parser.add_mutually_exclusive_group()
    mode_group.add_argument('--align-only', action='store_true',
                           help='Execute 16-pixel alignment only')
    mode_group.add_argument('--convert-only', action='store_true',
                           help='Execute PJPG conversion only (with complexity detection)')
    mode_group.add_argument('--combined', action='store_true', default=True,
                           help='Combined processing: align + convert (default)')
    mode_group.add_argument('--analyze-only', action='store_true',
                           help='Only analyze Alpha complexity, no conversion')

    # Complexity detection options
    complexity_group = parser.add_argument_group('complexity detection options')
    complexity_group.add_argument('--no-complexity-check', action='store_true',
                                help='Disable Alpha complexity detection (force convert all files)')
    complexity_group.add_argument('--complexity-threshold-high', type=float, default=0.1,
                                help='High complexity threshold (default: 0.1)')
    complexity_group.add_argument('--complexity-threshold-medium', type=float, default=0.05,
                                help='Medium complexity threshold (default: 0.05)')

    # Alignment options
    align_group = parser.add_argument_group('alignment options')
    align_group.add_argument('--pad-mode', choices=['transparent', 'edge', 'color'], default='transparent',
                           help='16-pixel alignment padding mode (default: transparent)')
    align_group.add_argument('--pad-color', default='transparent',
                           help='Padding color (hex #RRGGBB or #RRGGBBAA, or transparent/white/black)')

    # Naming options
    naming_group = parser.add_argument_group('naming options')
    naming_group.add_argument('--preserve-names', action='store_true', default=True,
                            help='Preserve original filenames without adding "_aligned" suffix (default: True)')
    naming_group.add_argument('--add-suffix', action='store_true',
                            help='Add "_aligned" suffix to output filenames')

    # JPEG quality options
    quality_group = parser.add_argument_group('JPEG quality options')
    quality_group.add_argument('-q', '--quality', type=int, default=85,
                              help='RGB JPEG compression quality (1-100, default 85)')
    quality_group.add_argument('-a', '--alpha-quality', type=int, default=85,
                              help='Alpha JPEG compression quality (1-100, default 85)')

    # Output control options
    output_group = parser.add_argument_group('output control options')
    output_group.add_argument('--pjpg-only', action='store_true',
                            help='Keep only PJPG files, clean up intermediate PNG files (only effective in combined mode)')

    # Other options
    misc_group = parser.add_argument_group('other options')
    misc_group.add_argument('--batch', action='store_true', default=True,
                          help='Batch processing mode (automatically enabled when input is directory)')
    misc_group.add_argument('--check-only', action='store_true',
                          help='Only check alignment status, no processing')

    args = parser.parse_args()

    # Parameter validation
    if not os.path.exists(args.input):
        print(f'❌ Error: Input path does not exist: {args.input}')
        return 1

    # Analysis mode doesn't require output path
    if not args.analyze_only and not args.output:
        print(f'❌ Error: Output path must be specified with -o/--output')
        return 1

    if args.quality < 1 or args.quality > 100 or args.alpha_quality < 1 or args.alpha_quality > 100:
        print(f'❌ Error: JPEG quality must be between 1-100')
        return 1

    # Parse padding color
    pad_color = parse_color(args.pad_color)

    # Handle naming options
    if args.add_suffix:
        preserve_names = False
    else:
        preserve_names = args.preserve_names

    # Create processor
    enable_complexity_check = not args.no_complexity_check
    processor = PNGProcessor(
        pad_mode=args.pad_mode,
        pad_color=pad_color,
        jpeg_quality=args.quality,
        alpha_quality=args.alpha_quality,
        enable_complexity_check=enable_complexity_check
    )

    # Set complexity thresholds
    if enable_complexity_check:
        processor.converter.analyzer.HIGH_COMPLEXITY_THRESHOLD = args.complexity_threshold_high
        processor.converter.analyzer.MEDIUM_COMPLEXITY_THRESHOLD = args.complexity_threshold_medium

        print(f'🧠 Alpha complexity detection enabled')
        print(f'   High complexity threshold: {args.complexity_threshold_high}')
        print(f'   Medium complexity threshold: {args.complexity_threshold_medium}')
        print(f'   Detection strategy: Automatically skip high complexity files, adjust medium complexity file parameters')
    else:
        print(f'⚠️  Alpha complexity detection disabled - will force convert all files')

    # Analysis mode only
    if args.analyze_only:
        if os.path.isfile(args.input):
            print(f'❌ Error: Analysis mode only supports directory input')
            return 1

        analysis_result = processor.analyze_directory_complexity(args.input, args.recursive)
        if analysis_result:
            print(f'\n✅ Complexity analysis completed!')
            print(f"📊 Total: {analysis_result['total']} files")
            print(f"❌ High risk: {len(analysis_result['high_risk'])} files")
            print(f"⚠️  Medium risk: {len(analysis_result['medium_risk'])} files")
            print(f"✅ Low risk: {len(analysis_result['low_risk'])} files")
        return 0

    # Check mode only
    if args.check_only:
        aligner = PNGAligner()
        if os.path.isfile(args.input):
            with Image.open(args.input) as img:
                aligned = aligner.check_alignment(img.width, img.height)
                print(f"{args.input}: {img.width}x{img.height} - {'✓ Aligned' if aligned else '✗ Not aligned'}")
        elif os.path.isdir(args.input):
            total = 0
            aligned_count = 0
            for root, dirs, files in os.walk(args.input):
                if not args.recursive and root != args.input:
                    continue
                for file in files:
                    if file.lower().endswith('.png'):
                        total += 1
                        try:
                            with Image.open(os.path.join(root, file)) as img:
                                aligned = aligner.check_alignment(img.width, img.height)
                                if aligned:
                                    aligned_count += 1
                                print(f"{file}: {img.width}x{img.height} - {'✓ Aligned' if aligned else '✗ Not aligned'}")
                        except Exception as e:
                            print(f'{file}: Error - {e}')
            print(f'\nSummary: {aligned_count}/{total} files are 16-pixel aligned')
        return 0

    # Execute processing
    success = False

    if args.align_only:
        success = processor.process_align_only(args.input, args.output, args.recursive, preserve_names)
    elif args.convert_only:
        success = processor.process_convert_only(args.input, args.output, args.recursive)
    else:  # combined mode (default)
        success = processor.process_combined(args.input, args.output, args.recursive, preserve_names, cleanup_png=args.pjpg_only)

    if success:
        print(f'\n🎉 Processing completed!')
        if enable_complexity_check and not args.align_only:
            print(f'💡 Tip: High complexity files have been copied as PNG format to target directory')
            print(f'         This ensures file integrity while avoiding ESP32-P4 hardware decoding timeout issues')
    else:
        print(f'\n❌ Processing failed!')
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())
