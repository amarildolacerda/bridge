from __future__ import annotations
import os

_dir = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(_dir, "dashboard.html")) as f:
    dashboard_html_content = f.read()

with open(os.path.join(_dir, "dashboard.css")) as f:
    dashboard_css_content = f.read()
