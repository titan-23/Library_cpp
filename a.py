import os


def create_rst_files(root_dir, output_dir):
    for subdir, _, files in os.walk(root_dir):
        relative_path = os.path.relpath(subdir, root_dir)
        rst_dir = os.path.join(output_dir, relative_path)

        # Create directories if they do not exist
        if not os.path.exists(rst_dir):
            os.makedirs(rst_dir)

        if rst_dir == "source/.":
            continue

        with open(os.path.join(rst_dir, "index.rst"), "w") as index_file:
            index_file.write(f"{relative_path}\n")
            index_file.write("=" * len(relative_path) + "\n\n")
            index_file.write(".. toctree::\n")
            index_file.write("   :maxdepth: 2\n\n")

            for file in files:
                if file.endswith((".h", ".cpp", ".hpp", ".c")):
                    file_path = os.path.relpath(os.path.join(subdir, file), root_dir)
                    rst_file_name = file.replace(".", "_") + ".rst"
                    rst_file_path = os.path.join(rst_dir, rst_file_name)

                    with open(rst_file_path, "w") as rst_file:
                        title = (
                            file.replace("_", " ")
                            .replace(".cpp", "")
                            .replace(".h", "")
                            .replace(".hpp", "")
                            .replace(".c", "")
                        )
                        rst_file.write(f"{title}\n")
                        rst_file.write("=" * len(title) + "\n\n")
                        rst_file.write(f".. doxygenfile:: titan_cpplib/{file_path}\n")

                    # Add the individual rst file to the index
                    index_file.write(f"   {rst_file_name}\n")


if __name__ == "__main__":
    root_dir = "titan_cpplib"
    output_dir = "source"
    create_rst_files(root_dir, output_dir)
