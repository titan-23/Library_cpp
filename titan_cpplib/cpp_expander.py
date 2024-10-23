# -*- coding: utf-8 -*-

# user settings --------------------------------------------------
# `./titan_cpplib` がある絶対パス
LIB_PATH = (
    "/mnt/c/Users/titan/source/Library_cpp/",
    "C:\\Users\\titan\\source\\Library_cpp\\",
    "/home/titan/source/Library_cpp/",
)
#  ---------------------------------------------------------------

from logging import getLogger, basicConfig
import os
import pyperclip
import shutil
import argparse

logger = getLogger(__name__)


def to_red(arg):
    return f"\u001b[31m{arg}\u001b[0m"


def to_green(arg):
    return f"\u001b[32m{arg}\u001b[0m"


def init_clipboard():
    for command in ["wl-clipboard", "xclip", "xsel"]:
        if shutil.which(command):
            pyperclip.set_clipboard(command)
            break


class CppExpander:

    def __init__(self, input_path: str) -> None:
        if not os.path.exists(input_path):
            logger.critical(to_red(f'input_path : "{input_path}" does not found.'))
            exit(1)
        self.input_path: str = input_path
        self.seen_path: set[str] = set()
        self.outputs: list[str] = []
        self.added_file = set()
        self.input_lines = 0

    def expand(self, output_fiepath: str) -> None:
        """コードを展開してoutput_fiepathに書き出す"""
        self.outputs = []
        self.get_code(self.input_path)
        output_code = "".join(self.outputs)
        if output_fiepath in ["clip"]:
            output_fiepath = "clipboard"
            pyperclip.copy(output_code)
        else:
            with open(output_fiepath, "w", encoding="utf-8") as f:
                f.write(output_code)
        logger.info(to_green("The process completed successfully."))
        logger.info(to_green(f"Output file: {output_fiepath} ."))

    def get_code(self, input_file_path: str) -> None:
        input_lines = 0
        with open(input_file_path, "r", encoding="utf-8") as input_file:
            for line in input_file:
                input_lines += 1
                if line.startswith(f'#include "titan_cpplib'):
                    _, s = line.split()
                    s = s.replace('"', "")
                    if s in self.added_file:
                        continue
                    self.added_file.add(s)
                    for lib_path in LIB_PATH:
                        new_path = f"{lib_path}{s}"
                        self.outputs.append(f"// {line}")
                        if os.path.exists(new_path):
                            logger.info(f"[include] \"{s.replace(lib_path, '')}\"")
                            self.get_code(new_path)
                            break
                    else:
                        logger.critical(f'File "{input_file_path}", line {input_lines}')
                        error_line = line.rstrip()
                        error_underline = "^" + "~" * (len(error_line) - 1)
                        logger.critical(to_red(f"    {error_line}"))
                        logger.critical(to_red(f"    {error_underline}"))
                        logger.critical(to_red(f"FileNotFoundError"))
                        exit(1)
                else:
                    self.outputs.append(line)
        if self.outputs and self.outputs[-1] != "\n":
            self.outputs.append("\n")


if __name__ == "__main__":
    basicConfig(
        format="%(asctime)s [%(levelname)s] : %(message)s",
        datefmt="%H:%M:%S",
        level=os.getenv("LOG_LEVEL", "INFO"),
    )
    init_clipboard()

    parser = argparse.ArgumentParser()
    parser.add_argument(
        "input_path",
    )
    parser.add_argument(
        "-o",
        "--output_path",
        default="clip",
        action="store",
    )

    args = parser.parse_args()

    expander = CppExpander(args.input_path)
    expander.expand(args.output_path)
