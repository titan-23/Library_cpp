# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

import os
import sys
sys.path.insert(0, os.path.abspath('../'))
sys.path.insert(0, os.path.abspath('../../'))
sys.path.insert(0, os.path.abspath('../titan_cpplib/'))
sys.path.insert(0, os.path.abspath('../titan_cpplib/algorithm/'))
# sys.path.insert(0, os.path.abspath('../titan_cpplib/data_structures/'))
# sys.path.insert(0, os.path.abspath('../titan_cpplib/graph/'))
# sys.path.insert(0, os.path.abspath('../titan_cpplib/io/'))
# sys.path.insert(0, os.path.abspath('../titan_cpplib/math/'))
# sys.path.insert(0, os.path.abspath('../titan_cpplib/string/'))
autodoc_typehints = 'signature'  # 型ヒントを有効
autodoc_default_options = {
    'private-members': False,
    'show-inheritance': False,
    'members': None,
    'maxdepth': 1,
}

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = 'titan_cpplib'
copyright = '2024, titan'
author = 'titan'

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

extensions = [
    'sphinx.ext.autodoc',
    'sphinx.ext.autosummary',
    'sphinx.ext.todo',
    'sphinx.ext.viewcode',
    'sphinx.ext.napoleon',
    'recommonmark',
    'sphinx.ext.githubpages',
    'sphinx_copybutton',
    'breathe',
]

# sphinx_copybutton設定
copybutton_prompt_text = ">>> "

viewcode_line_numbers = True

# Napoleon settings
napoleon_google_docstring = True
napoleon_include_init_with_doc = False
napoleon_include_private_with_doc = False
napoleon_include_special_with_doc = True
napoleon_use_admonition_for_examples = False
napoleon_use_admonition_for_notes = False
napoleon_use_admonition_for_references = False
napoleon_use_ivar = False
napoleon_use_param = True
napoleon_use_rtype = True
napoleon_preprocess_types = False
napoleon_type_aliases = None
napoleon_attr_annotations = True

# Breathe拡張モジュールの設定
breathe_projects = {
  "My Project": "/mnt/c/Users/titan/source/Library_cpp/_docs_cpp/xml"
}
breathe_default_project = "titan_cpplib"

templates_path = ['_templates']
exclude_patterns = ['_build', 'Thumbs.db', '.DS_Store']

# language = 'ja'
language = 'en'

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

# html_theme = 'furo'
html_theme = 'sphinx_rtd_theme'

html_static_path = ['_static']
html_js_files = ['_static/script.js']
html_css_files = ['_static/style.css']
