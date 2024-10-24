make clean
rm ./docs/ -r
rm ./docs_doxygen/ -r
rm ./titan_cpplib_expanded/ -r

python3 all_cpp_expander.py
doxygen Doxyfile
python3 a.py
make html

cp -r ./build/html/ ./docs/
