from pprint import pprint

from bs4 import BeautifulSoup
import requests

response = requests.get("https://tests.stockfishchess.org/tests/view/66413b7df9f4e8fc783c9bbb")
with open(f"spsa-3072.html", "w") as f:
    spsa_html = response.text
    f.write(spsa_html)

soup = BeautifulSoup(spsa_html, "html.parser")
spsa_params_table = soup.find_all("table")[1]
variables_txt = ""

var_names = {}
for row in spsa_params_table.find_all("tr", class_="spsa-param-row"):
    td = row.find_all("td")
    var_name = td[0].text.split("[")[0]
    if not var_names.get(var_name):
        var_names[var_name] = []
    var_names[var_name].append(int(float(td[1].text)))

output_strs = ""
for var_name, values in var_names.items():
    output_strs += f"int {var_name}[{len(values)}] = {{"
    output_strs += f"{', '.join([str(i) for i in values])}"
    output_strs += f"}};\n\n"

print(output_strs.strip())
