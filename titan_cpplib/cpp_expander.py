# user settings --------------------------------------------------
SLASH = '/'
LIB_PATH = '/mnt/c/Users/titan/source/Library_cpp'
# LIB_PATH = 'C:\\Users\\titan\\source\\Library_cpp'
TO_LIB_PATH = '.'
#  ---------------------------------------------------------------

import sys
import pyperclip
import re

def print_red(arg):
  print(f'\u001b[31m{arg}\u001b[0m')

def get_code(now_path, input_file, is_input=False):
  global input_lines, output

  for line in input_file:
    if is_input:
      input_lines += 1
    if line.startswith(f'#include "titan_cpplib{SLASH}'):
      _, s = line.split()
      s = s.replace('\"', '')
      s = s.replace('\'', '')
      s = f'{LIB_PATH}{SLASH}{s}'
      if s not in added_file:
        output += f'// {line}'
        try:
          f = open(s, 'r', encoding='utf-8')
        except FileNotFoundError:
          print(f'File \"{input_filename}\", line {input_lines}')
          error_line = line.rstrip()
          error_underline = '^' + '~' * (len(error_line)-1)
          print_red(f'    {error_line}')
          print_red(f'    {error_underline}')
          print(f"FileNotFoundError")
          exit(1)
        get_code(s, f)
        print(f"#include \"{s.replace(LIB_PATH, '')}\" OK.")
        f.close()
        added_file.add(s)
    else:
      output += line

if __name__ == '__main__':

  input_filename = sys.argv[1]
  output_filename = sys.argv[2] if len(sys.argv) == 3 else 'clip'
  input_file = open(input_filename, 'r', encoding='utf-8')

  added_file = set()
  output = ''
  input_lines = 0

  get_code(TO_LIB_PATH, input_file, is_input=True)

  if output_filename in ['clip', 'CLIP', 'c', 'C']:
    output_filename = 'clipboard'
    pyperclip.copy(output)
  else:
    output_file = open(output_filename, 'w', encoding='utf-8')
    print(output, file=output_file)
    output_file.close()

  print()
  print('The process completed successfully.')
  print(f'Output file: \"{output_filename}\" .')
