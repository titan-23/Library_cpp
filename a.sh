make clean
rm ./docs/ -r
rm ./docs_doxygen/ -r

doxygen Doxyfile
python3 a.py
make html

cp -r ./build/html/ ./docs/
