#!/usr/bin/env python
import sys
import os

# Locate MOOSE directory
MOOSE_DIR = os.getenv('MOOSE_DIR', os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'moose')))
if not os.path.exists(os.path.join(MOOSE_DIR, 'python')):
    MOOSE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', 'moose'))
if not os.path.exists(os.path.join(MOOSE_DIR, 'python')):
    raise Exception('Failed to locate MOOSE, specify the MOOSE_DIR environment variable.')
os.environ['MOOSE_DIR'] = MOOSE_DIR

# Add THM_DIR
if 'THM_DIR' not in os.environ:
    os.environ['THM_DIR'] = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))

# Append MOOSE python directory
MOOSE_PYTHON_DIR = os.path.join(MOOSE_DIR, 'python')
if MOOSE_PYTHON_DIR not in sys.path:
    sys.path.append(MOOSE_PYTHON_DIR)

# load MooseDocs extensions
sys.path.append(os.path.join(os.environ['THM_DIR'], 'python', 'MooseDocs', 'extensions'))

from MooseDocs import main
if __name__ == '__main__':
    sys.exit(main.run())
