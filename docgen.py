import json

j = json.load(open("_out.json", "r"))

for k, v in j.items():
    # print(v)
    if v["kind"] == "module":
        # print(v)
        print(v["path"])
        for n, c, d in [(j[c]["name"],j[c]["pos"][0],j[c].get("doc","")) for c in v["children"] if j[c]["kind"]=="class"]:
            print("-", c, n)
            if d:
                print(d)

