from pprint import pprint

from bs4 import BeautifulSoup
import requests

with open("search.cpp", "r") as f:
    search_cpp = f.read()

with open("spsa-variables-cpp.txt", "r") as f:
    spsa_cpp = f.read()

with open("search.cpp", "w") as f:
    f.write(
        search_cpp.split("// spsa vars start")[0] +
        "// spsa vars start\n" +
        spsa_cpp +
        "// spsa vars end" +
        search_cpp.split("// spsa vars end")[1]
    )
