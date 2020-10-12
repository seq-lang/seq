#%%
import json
import itertools
import os
import os.path
import sys
import subprocess as sp

from sphinxcontrib.napoleon.docstring import GoogleDocstring
from sphinxcontrib.napoleon import Config

def loadJson(directory):
    files = []
    for r, _, fs in os.walk(directory):
        for f in fs:
            files.append(os.path.abspath(os.path.join(r, f)))
    files = '\n'.join(files)
    s = sp.run(['build/seqc', '-docstr'], stdout=sp.PIPE, input=files.encode('utf-8'))
    if s.returncode != 0:
        raise ValueError('seqc failed')
    return json.loads(s.stdout.decode('utf-8'))

j = loadJson(sys.argv[1])

modules = {k: v["path"] for k, v in j.items() if v["kind"] == "module"}
pref = os.path.commonprefix(list(modules.values()))

modules = {k: v[len(pref):-4].replace("/", ".") for k, v in modules.items()}

idToModule = {}
for mk, m in modules.items():
    for i in j[mk]['children']:
        idToModule[i] = mk
        if 'members' in j[i]:
            for k in j[i]['members']:
                idToModule[k] = mk
        if 'children' in j[i]:
            for k in j[i]['children']:
                idToModule[k] = mk


# napoleon_config = Config(napoleon_use_param=True, napoleon_use_rtype=True)
def parseDocstr(s, level=1):
    # l = GoogleDocstring(s, napoleon_config).lines()
    # return '\n'.join(s)
    s = s.split('\n')
    while s and s[0] == '':
        s = s[1:]
    while s and s[-1] == '':
        s = s[:-1]
    if not s:
        return ''
    i = 0
    indent = len(list(itertools.takewhile(lambda i: i == ' ', s[0])))
    return '\n'.join(
        ('   ' * level) + l[indent:]
        for l in s
    )

def getLink(h):
    if h[0].isdigit():
        # if h in idToModule:
        #     f = modules[idToModule[h]]
        #     return f'{j[h]["name"]}'
        return f'{j[h]["name"]}'
    return f"{h}"

def parseType(a):
    s = ''
    if isinstance(a, list):
        head, tail = a[0], a[1:]
    else:
        head, tail = a, []
    s += getLink(head)
    if tail:
        for ti, t in enumerate(tail):
            s += "[" if not ti else ", "
            s += parseType(t)
        s += "]"
    return s

def parseFunction(v):
    s = ""
    if 'generics' in v and v['generics']:
        s += f'[{", ".join(v["generics"])}]'
    s += "("
    for ai, a in enumerate(v['args']):
        s += "" if not ai else ", "
        s += f'{a["name"]}'
        if "type" in a:
            s += " : " + parseType(a["type"])
        if "default" in a:
            s += " = " + a["default"] + ""
    s += ')'
    if "ret" in v:
        s += " -> " + parseType(v["ret"])
    # if "extern" in v:
    #     s += f" (_{v['extern']} function_)"
    # s += "\n"
    return s


mdls = []
for mk, m in modules.items():
    # if m != 'statistics':
        # continue
    l = m.split('.')
    l, r = '/'.join(l[:-1]), l[-1]
    os.system(f"mkdir -p docs/sphinx/stdlib/{l}")
    with open(f"docs/sphinx/stdlib/{l}/{r}.rst", "w") as f:
        mdls.append(f"stdlib/{r if l == '' else l+'/'+r}")
        f.write(f".. seq:module:: {m}\n")
        f.write(f":seq:mod:`{m}`\n")
        f.write(f"========\n\n")

        f.write(f"Source code: `{j[mk]['path']} <{m.split('.')[-1]}.seq>`_\n\n")
        if 'doc' in j[mk]:
            f.write(parseDocstr(j[mk]['doc']) + "\n")

        for i in j[mk]['children']:
            v = j[i]

            if v['kind'] == 'class' and v['type'] == 'extension':
                v['name'] = j[v['parent']]['name']
            if v['name'].startswith('_'):
                continue

            if v['kind'] == 'class':
                if v['name'].endswith('Error'):
                    v["type"] = "exception"
                f.write(f'.. seq:{v["type"]}:: {v["name"]}')
                if 'generics' in v and v['generics']:
                    f.write(f'[{", ".join(v["generics"])}]')
            elif v['kind'] == 'function':
                f.write(f'.. seq:function:: {v["name"]}{parseFunction(v)}')
            elif v['kind'] == 'variable':
                f.write(f'.. seq:data:: {v["name"]}')

            # if v['kind'] == 'class' and v['type'] == 'extension':
            #     f.write(f'**`{getLink(v["parent"])}`**')
            # else:
            # f.write(f'{m}.**`{v["name"]}`**')
            f.write("\n")

            # f.write("\n")
            # if v['kind'] == 'function' and 'attrs' in v and v['attrs']:
            #     f.write("**Attributes:**" + ', '.join(f'`{x}`' for x in v['attrs']))
            #     f.write("\n")
            if 'doc' in v:
                f.write("\n" + parseDocstr(v['doc']) + "\n")
            f.write("\n")

            if v['kind'] == 'class':
                # if 'args' in v and any(c['name'][0] != '_' for c in v['args']):
                #     f.write('#### Arguments:\n')
                #     for c in v['args']:
                #         if c['name'][0] == '_':
                #             continue
                #         f.write(f'- **`{c["name"]} : `**')
                #         f.write(parseType(c["type"]) + "\n")
                #         if 'doc' in c:
                #             f.write(parseDocstr(c['doc'], 1) + "\n")
                #         f.write("\n")

                mt = [c for c in v['members'] if j[c]['kind'] == 'function']

                props = [c for c in mt if 'property' in j[c].get('attrs', [])]
                # if props:
                #     f.write('#### Properties:\n')
                #     for c in props:
                #         v = j[c]
                #         f.write(f'- <a name="{c}"></a> **`{v["name"]}`**')
                #         if 'ret' in v:
                #             f.write('` : `' + parseType(v['ret']))
                #         if 'doc' in v:
                #             f.write(parseDocstr(v['doc'], 1) + "\n")
                #         f.write("\n")

                magics = [c for c in mt if len(j[c]['name']) > 4 and j[c]['name'].startswith('__') and j[c]['name'].endswith('__')]
                if magics:
                    f.write('   **Magic methods:**\n\n')
                    for c in magics:
                        v = j[c]
                        f.write(f'      .. seq:method:: {v["name"]}{parseFunction(v)}\n')
                        if 'doc' in v:
                            f.write("\n" + parseDocstr(v['doc'], 4) + "\n\n")
                        f.write("\n")
                methods = [c for c in mt if j[c]['name'][0] != '_' and c not in props]
                if methods:
                    f.write('   **Methods:**\n\n')
                    for c in methods:
                        v = j[c]
                        f.write(f'      .. seq:method:: {v["name"]}{parseFunction(v)}\n')
                        if 'doc' in v:
                            f.write("\n" + parseDocstr(v['doc'], 4) + "\n\n")
                        f.write("\n")
            f.write("\n\n")

with open(f"docs/sphinx/stdlib/index.rst", "w") as fc:
    fc.write("Seq standard library\n")
    fc.write("====================\n\n")
    fc.write(".. toctree::\n\n")
    for m in sorted(set(mdls)):
        fc.write(f"   {m}\n")
