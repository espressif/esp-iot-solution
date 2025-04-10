# -*- coding: utf-8 -*-
#
# Common (non-language-specific) configuration for Sphinx
#
# This file is imported from a language-specific conf.py (ie en/conf.py or
# zh_CN/conf.py)

#ESP_DOCS_PATH = os.environ['ESP_DOCS_PATH']

from esp_docs.conf_docs import *  # noqa: F403,F401

# format: {tag needed to include: documents to included}, tags are parsed from sdkconfig and peripheral_caps.h headers

extensions += [ 'sphinx_copybutton',
                'sphinxcontrib.wavedrom',
                'esp_docs.esp_extensions.dummy_build_system',
                'esp_docs.esp_extensions.run_doxygen',
                ]
# link roles config
github_repo = 'espressif/esp-iot-solution'

# context used by sphinx_idf_theme
html_context['github_user'] = 'espressif'
html_context['github_repo'] = 'esp-iot-solution'

languages = ['en', 'zh_CN']

project_homepage = 'https://github.com/espressif/esp-iot-solution'

# The name of an image file (relative to this directory) to place at the top
# of the sidebar.
html_logo = '../_static/espressif-logo.svg'

# Add any paths that contain custom static files (such as style sheets) here,
# relative to this directory. They are copied after the builtin static files,
# so a file named "default.css" will overwrite the builtin "default.css".
html_static_path = ['../_static']
html_css_files = ['js/chatbot_widget.css']

versions_url = './_static/js/generic_version.js'

# Extra options required by sphinx_idf_theme
project_slug = 'esp-iot-solution'

# The name of the Pygments (syntax highlighting) style to use.
pygments_style = 'sphinx'

# List of patterns, relative to source directory, that match files and
# directories to ignore when looking for source files.
exclude_patterns = ['_build','README.md']

# Final PDF filename will contains target and version
pdf_file_prefix = u'esp-iot-solution'

# The name of an image file (relative to this directory) to place at the top of
# the title page.
latex_logo = '../_static/espressif2.pdf'

# add Tracking ID for Google Analytics
google_analytics_id = 'G-YK0H573CWX'
