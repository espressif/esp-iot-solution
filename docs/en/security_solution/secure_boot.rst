Secure Boot
*****************

Secure Boot is a critical security mechanism in ESP32 series chips that ensures devices only execute authorized firmware through digital signature verification during boot process and OTA updates. This mechanism establishes a complete chain of trust from bootloader to application, providing comprehensive code integrity protection for devices.

Key features of Secure Boot:

- **Code Integrity Protection**: Ensures firmware code has not been tampered with or replaced through cryptographic verification, preventing execution of unauthorized malicious programs
- **Complete Chain of Trust**: Establishes an end-to-end verification mechanism from bootloader to application, ensuring every step in the boot process is verified
- **Multiple Signature Algorithms**: Supports RSA-PSS or ECDSA as signature algorithms, with hardware acceleration for efficient verification
- **Hardware Security Storage**: Stores signature key digests in protected eFuse, providing strict hardware-level read/write protection mechanisms

For more detailed information, refer to: `ESP-IDF Secure Boot v2 Documentation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html>`_

Secure Boot and Flash Encryption
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Secure Boot and Flash Encryption are complementary security features:

- **Secure Boot**: Ensures code integrity, prevents running unauthorized firmware
- **Flash Encryption**: Protects data confidentiality, prevents reading Flash code and data

It is recommended to enable both features in production environments for optimal security protection. For details, refer to: `Secure Boot and Flash Encryption <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html#secure-boot-flash-encryption>`_

Secure Boot Chain of Trust
~~~~~~~~~~~~~~~~~~~~~~~~~~

Secure Boot verifies the following software components:

- First-stage bootloader (ROM code) as chip-embedded code that cannot be modified or compromised, serving as the foundation of the trust chain
- First-stage bootloader verifies the integrity and authenticity of the second-stage bootloader through digital signature
- Second-stage bootloader verifies the integrity and authenticity of the application binary through digital signature
- Application verifies the digital signature of new firmware during OTA update process, ensuring update security

Detailed verification process: `Secure Boot v2 Process <https://docs.espressif.com/projects/esp-idf/en/latest/esp32/security/secure-boot-v2.html#secure-boot-v2-process>`_

Secure Boot Feature Support List
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. list-table:: Secure Boot Feature Support List
    :header-rows: 1

    * - Chip Model
      - Supported Algorithms (Curve/Bits)
      - Signature Verification Time (CPU Frequency)
      - Maximum Number of Public Keys
      - Notes/Special Instructions
    * - ESP32 (ECO V3+)
      - RSA-3072 (RSA-PSS, Secure Boot V2)<br>AES (V1)
      - Not specifically stated
      - 1
      - Only supports 1 public key, Secure Boot V2 requires ECO V3 or above
    * - ESP32-S2
      - RSA-3072 (RSA-PSS)
      - Not specifically stated
      - 3
      - Up to 3 public keys, revocable
    * - ESP32-S3
      - RSA-3072 (RSA-PSS)
      - Not specifically stated
      - 3
      - Up to 3 public keys, revocable
    * - ESP32-C2
      - ECDSA-192 (NISTP192)<br>ECDSA-256 (NISTP256, recommended)
      - Not specifically stated
      - 1
      - Only supports ECDSA, and only 1 public key `ESP32-C2 Kconfig Reference <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c2/api-reference/kconfig-reference.html#config-secure-boot-ecdsa-key-len-size>`_
    * - ESP32-C3 (v0.3 and above)
      - RSA-3072 (RSA-PSS)
      - Not specifically stated
      - 3
      - Up to 3 public keys, revocable
    * - ESP32-C5
      - RSA-3072<br>ECDSA-256 (P-256)<br>ECDSA-384 (P-384)
      - RSA-3072: 12.1 ms<br>ECDSA-256: 5.6 ms<br>ECDSA-384: 20.6 ms (48 MHz)
      - 3
      - Only one algorithm can be selected `ESP32-C5 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c5/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-C6
      - RSA-3072<br>ECDSA-256 (P-256)
      - RSA-3072: 10.2 ms<br>ECDSA-256: 83.9 ms (40 MHz)
      - 3
      - Only one algorithm can be selected `ESP32-C6 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32c6/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-H2
      - RSA-3072<br>ECDSA-256 (P-256)
      - RSA-3072: 18.3 ms<br>ECDSA-256: 76.2 ms (32 MHz)
      - 3
      - Only one algorithm can be selected `ESP32-H2 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32h2/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_
    * - ESP32-P4
      - RSA-3072<br>ECDSA-256 (P-256)
      - RSA-3072: 14.8 ms<br>ECDSA-256: 61.1 ms (40 MHz)
      - 3
      - Only one algorithm can be selected `ESP32-P4 Secure Boot v2 <https://docs.espressif.com/projects/esp-idf/en/latest/esp32p4/security/secure-boot-v2.html#secure-boot-v2-scheme-selection>`_

**Notes:**
- "Maximum Number of Public Keys" refers to the number of public keys that can be stored in eFuse, some chips support key revocation.
- "Signature Verification Time" refers to the time required for the ROM bootloader to verify the bootloader image signature, actual boot time may vary slightly.
- For more detailed configuration and usage recommendations, please refer to the official documentation for the corresponding chip.

Signature and Key Management
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In information security, digital signature is an integrity verification mechanism based on asymmetric encryption. Its core purpose is to: prove the authenticity of the data source (Authenticity), and ensure that data has not been tampered with during transmission or storage (Integrity).

The signature process uses a pair of keys:

- Private Key: Used to sign data, the private key must be kept strictly confidential, stored in a secure environment (local firmware development environment or remote signing server).
- Public Key: Used to verify the legitimacy of signatures, the public key can be safely distributed to any entity that needs to verify signatures.

Secure Boot v2 supports various asymmetric encryption algorithms, depending on the chip model:

- **RSA-PSS**: RSA-3072 key, suitable for scenarios requiring fast boot
- **ECDSA**: Supports P-256 and P-192 curves, shorter key length but longer verification time (limited by current ECDSA hardware acceleration capabilities)

Signature Block Format
~~~~~~~~~~~~~~~~~~~~~~

The signature block is a critical structure in ESP32-S3 Secure Boot v2 used to verify image integrity and source trustworthiness. It is a fixed-format data block appended to the end of the bootloader or application image, containing signature information, public keys, and auxiliary verification fields.

- The signature block mainly contains the SHA-256 hash value of the image, public key, signature result, etc.
- The signature block itself occupies 1216 bytes, located at the 4KB-aligned boundary at the end of the image, and has CRC32 checksum
- Each image can have up to 3 signature blocks, used to support multi-key verification and revocation mechanisms

Detailed signature block definition: `Signature Block Format <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#signature-block-format>`_

Enabling Hardware Secure Boot
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Method 1: Enable via configuration items (menuconfig)**:

The simplest way to enable Secure Boot v2 in ESP-IDF is through ``menuconfig`` configuration items. When building the ``bootloader``, the build system automatically inserts the signature process when compiling the ``bootloader`` and ``app`` images. During first boot, the bootloader automatically burns the eFuse to enable Secure Boot and revokes unused key slots. This method is suitable for the development phase, with automated and highly integrated processes, but since key revocation is irreversible, it should be used with caution in mass production.

1. Open the project configuration menu ``Security features`` and enable ``Enable hardware Secure Boot in bootloader``
2. Select the ``Secure Boot v2`` signature scheme, depending on the chip, the signature algorithm options are ``RSA`` or ``ECDSA``
3. Specify the signing key file path, the key file (e.g., RSA 3072) can be generated using the ``idf.py secure-generate-signing-key`` or ``openssl genrsa`` command
4. Using the ``idf.py build`` command will directly compile, align, pad, and generate the ``app`` image with attached signature blocks
5. Use the ``idf.py bootloader`` command to generate a bootloader image with Secure Boot enabled and signature blocks attached
6. For security reasons, by default, a bootloader with Secure Boot enabled needs to be burned separately using the ``esptool.py write_flash`` command, ``idf.py flash`` can only burn application and partition table partitions
7. Restart the device, the bootloader will automatically burn the eFuse to enable Secure Boot on first run, write the calculated public key digest of the signature block to eFuse, and revoke unused key slots
8. In subsequent boot processes, the public key digest in eFuse will be used to verify the signatures of bootloader and app images, ensuring the device only runs firmware signed with the corresponding private key

**Notes**: After enabling Secure Boot, the bootloader image size will increase and may require readjustment of partition table size. The private key must be safeguarded; if the private key is leaked, attackers can generate malicious firmware with valid signatures, and if the key is lost, the device cannot update firmware.

**Method 2: Enable via external tools**:

Another method is to manually configure eFuse before burning using the ``espefuse.py`` tool, including writing public key digest, setting key usage, enabling the ``Secure Boot`` flag, etc. This method does not trigger the bootloader's automatic key revocation logic, thus preserving unused key slots, facilitating future key rotation or OTA signature updates. It is suitable for mass production environments, with high security and fine control, but complex operations requiring strict key management and burning processes. Additionally, you can use `remote signing <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#remote-signing-of-images>`_ or `external HSM <https://docs.espressif.com/projects/esptool/en/latest/esp32c2/espsecure/index.html#remote-signing-using-an-external-hsm>`_ to generate signature blocks, further enhancing private key security.

1. Generate private key (can be generated locally or in a remote environment): Create a private key for signing firmware
2. Generate public key digest: Calculate the SHA-256 hash of the public key
3. Burn digest: Write the public key digest to specific eFuse area
4. Enable Secure Boot: Set related eFuse flag bits
5. Revoke unused key slots: Prevent unauthorized keys from being added
6. Burn security configuration: Set other security-related eFuse bits
7. Configure project: Disable automatic signing to use external signing tools
8. Sign images: Sign bootloader and app
9. Burn images: Write signed firmware to the device
10. Enable secure download: Finally lock all security configurations

For detailed configuration methods and instructions, see: `Enable v2 Externally <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/security-features-enablement-workflows.html#enable-secure-boot-v2-externally>`_

Key Revocation
~~~~~~~~~~~~~~

For chips with multiple public keys burned in, keys can be revoked to prevent exploiting leaked or unused keys. After revoking a key, firmware signed with that key will fail verification, thereby protecting the device from potential attacks.

**Implementation Prerequisites**:

Key revocation functionality requires the following prerequisites:

1. The chip must support multiple public keys (>1) and have key revocation capability
2. At least two public key digests are burned into eFuse at factory (e.g., key #0 and key #1)
3. The second-stage bootloader (bootloader) uses a multi-signature mechanism, signed with private keys corresponding to key #0 and key #1
4. The application (app) is signed using only one of the keys (e.g., key #0)

For more information on multiple signatures, please refer to: `Multiple Keys <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#multiple-keys>`_

**Implementation Steps**:

1. OTA firmware signed with a new key (e.g., key #1) is written to the backup partition after verification by the current application
2. After the new application starts, it verifies the second-stage bootloader (bootloader) signature is normal (confirming bootloader key #1 is available)
3. Call `esp_ota_revoke_secure_boot_public_key() <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/api-reference/system/ota.html#_CPPv437esp_ota_revoke_secure_boot_public_key38esp_ota_secure_boot_public_key_index_t>`__ to revoke the old key (key #0 revoked)
4. After the old key (key #0) is revoked, firmware signed with that key will fail verification
5. Both the second-stage bootloader (bootloader) and application (app) use the new key (key #1) for signature verification

Key revocation details: `Key Revocation <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#key-revocation>`_

Restrictions After Enabling
~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Security Restrictions**:

- Once hardware Secure Boot is enabled, there is no way to disable it
- Updated bootloaders or applications must be signed with matching keys
- The USB-OTG USB stack is disabled, preventing updates via serial emulation or DFU
- Further eFuse read protection is disabled, preventing attackers from read-protecting the Secure Boot public key digest

**Debug Interfaces**:

- JTAG interface is disabled by default
- UART download mode switches to secure mode

Detailed restriction information: `Restrictions After Secure Boot is Enabled <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#restrictions-after-secure-boot-is-enabled>`_

Software Signature Verification
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Software signature verification provides a lightweight signature verification mechanism, using the same signature scheme as hardware Secure Boot, but only verifies new images during OTA updates. It is suitable for scenarios sensitive to boot speed or with lower physical security requirements, but does not provide complete boot chain protection.

**Software Signature vs Hardware Secure Boot**:

.. list-table:: Software Signature vs Hardware Secure Boot
    :header-rows: 1
    :widths: 20 40 40

    * - Item
      - 🛡️ Secure Boot v2 (Hardware)
      - 🔓 Software Signature Verification (No Secure Boot)
    * - Boot-time Verification
      - ✅ Verifies bootloader and app image signatures
      - ❌ Does not verify current app, assumes it is trusted
    * - OTA Update Verification
      - ✅ Uses keys in eFuse to verify new app signature
      - ✅ Uses public key in current app's signature block to verify new app
    * - Root of Trust
      - Public key digest burned in eFuse
      - Public key in the signature block of current running app
    * - eFuse Configuration Requirements
      - Must burn SECURE_BOOT_EN and KEY_DIGESTx
      - No need to burn Secure Boot related eFuse
    * - Key Revocation Mechanism
      - ✅ Supports KEY_REVOKE_x and aggressive revocation policy
      - ❌ Does not support key revocation
    * - Signature Block Support Count
      - ✅ Up to 3 signature blocks, supports multi-key verification
      - ❌ Only uses the first signature block, ignores others
    * - Tamper Resistance
      - ✅ Prevents Flash replacement or malicious code injection
      - ❌ Cannot prevent physical attacks or Flash replacement
    * - Boot Performance
      - ❌ Has signature verification overhead during boot
      - ✅ Faster boot, no verification delay
    * - Development Convenience
      - ❌ Irreversible after enabling eFuse, requires careful operation
      - ✅ No irreversible operations, suitable for development and debugging
    * - Applicable Scenarios
      - Mass production deployment, high-security requirements devices
      - Development testing, boot speed sensitive, physically controlled environments
    * - Key Management Flexibility
      - ✅ Supports multiple key rotation and revocation
      - ❌ Only relies on public key of current app, cannot rotate
    * - Recommendation
      - ✅ Officially recommended for formal products
      - ⚠️ Use only with clear threat model, needs careful evaluation

**Configuration Method**:

Enable the ``CONFIG_SECURE_SIGNED_APPS_NO_SECURE_BOOT`` option.

For details, refer to: `Signed App Verification Without Hardware Secure Boot <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#signed-app-verification-without-hardware-secure-boot>`_

Example Code
~~~~~~~~~~~~

For complete Secure Boot usage examples, please refer to:

- `ESP-IDF Secure Boot Example <https://github.com/espressif/esp-idf/tree/master/tools/test_apps/security/secure_boot>`_
- `Security Features Comprehensive Example <https://github.com/espressif/esp-idf/tree/master/examples/security/security_features_app>`_

These examples demonstrate:

- Secure Boot status check
- Signature key generation and management
- Multi-key signing and revocation
- Use in conjunction with Flash Encryption

Best Practices
~~~~~~~~~~~~~~

1. **Use high-quality entropy sources to generate signing keys**
2. **Always keep signing keys private**
3. **Avoid third-party observation of key generation or signing process**
4. **Enable all Secure Boot configuration options**
5. **Use in conjunction with Flash Encryption**
6. **Use multiple keys to reduce single point of failure**
7. **Establish key rotation policy**

For more best practices: `Secure Boot Best Practices <https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/security/secure-boot-v2.html#secure-boot-best-practices>`_

Frequently Asked Questions (FAQ)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* Please refer to: `ESP-FAQ Security Section <https://docs.espressif.com/projects/esp-faq/en/latest/software-framework/security.html>`_
