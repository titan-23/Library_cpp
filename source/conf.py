# Configuration file for the Sphinx documentation builder.
#
# For the full list of built-in configuration values, see the documentation:
# https://www.sphinx-doc.org/en/master/usage/configuration.html

# -- Project information -----------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#project-information

project = "titan_cpplib"
copyright = "2024, titan23"
author = "titan23"

# -- General configuration ---------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#general-configuration

import os
import sys

sys.path.insert(0, os.path.abspath("."))
sys.path.insert(0, os.path.abspath("./../docs_doxygen/"))
sys.path.insert(0, os.path.abspath("./../docs_doxygen/xml"))
sys.path.insert(0, os.path.abspath("./../"))

autodoc_typehints = "signature"  # 型ヒントを有効
autodoc_default_options = {
    "private-members": False,
    "show-inheritance": False,
    "members": None,
    "maxdepth": 0,
}

extensions = [
    "breathe",
    "sphinx.ext.autodoc",
    "sphinx.ext.autosummary",
    "sphinx.ext.todo",
    "sphinx.ext.viewcode",
    "sphinx.ext.napoleon",
    "recommonmark",
    "sphinx.ext.githubpages",
    "sphinx_copybutton",
]

breathe_projects = {"titan_cpplib": os.path.abspath("./../docs_doxygen/xml")}
breathe_default_project = "titan_cpplib"
breathe_default_members = ('members',)

templates_path = ["_templates"]
exclude_patterns = []

language = "en"

pygments_style = 'sphinx'

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

# -- Options for HTML output -------------------------------------------------
# https://www.sphinx-doc.org/en/master/usage/configuration.html#options-for-html-output

html_theme = "furo"
html_static_path = ["_static"]
html_css_files = ["_static/style.css"]
