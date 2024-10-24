from typing import List
import os
import re
from logging import getLogger, basicConfig
from titan_cpplib.cpp_expander import CppExpander

logger = getLogger(__name__)

if __name__ == "__main__":
    basicConfig(
        format="%(asctime)s [%(levelname)s] : %(message)s",
        datefmt="%H:%M:%S",
        level=os.getenv("LOG_LEVEL", "INFO"),
    )

    banned_name = {}
    banned_dirs = {}

    for cur_dir, dirs, files in os.walk("./titan_cpplib/"):
        for file in files:
            if not file.endswith(".cpp"):
                continue
            if file in banned_name:
                continue
            if cur_dir in banned_dirs:
                continue
            logger.info(f"expanding {cur_dir} {file}")
            input_path = os.path.join(cur_dir, file)
            output_dir = os.path.join(cur_dir)
            output_dir = (
                "./titan_cpplib_expanded/" + output_dir[len("./titan_cpplib/") :]
            )
            output_path = (
                "./titan_cpplib_expanded/" + input_path[len("./titan_cpplib/") :]
            )
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)

            with open(output_path, "w", encoding="utf-8") as output_file:
                f = input_path.lstrip("./").replace("/", ".")
                output_file.write(f"#include \"{input_path[2:]}\"\n")
            expander = CppExpander(output_path)
            try:
                expander.expand(output_path)
            except SystemExit:
                logger.error(f"展開に失敗しました: {expander.input_path}")
                with open(output_path, "w", encoding="utf-8") as output_file:
                    with open(input_path, "r", encoding="utf=8") as input_file:
                        output_file.write("# 展開に失敗しました\n")
                        for line in input_file:
                            output_file.write(line)
