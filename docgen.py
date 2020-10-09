#%%
import json
import itertools
import os
import os.path
import sys
import subprocess as sp

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

def parseDocstr(s, level=0):
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
        ('  ' * level) + l[indent:]
        for l in s
    )

def getLink(h):
    if h[0].isdigit():
        if h in idToModule:
            f = modules[idToModule[h]]
            return f'[`{j[h]["name"]}`]({f}.md#{h})'
        return f'`{j[h]["name"]}`'
    return f"`{h}`"

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
    s = "("
    for ai, a in enumerate(v['args']):
        s += "" if not ai else ", "
        s += f'`{a["name"]}`'
        if "type" in a:
            s += " `:` " + parseType(a["type"])
        if "default" in a:
            s += " `=` `" + a["default"] + "`"
    s += ')'
    if "ret" in v:
        s += " `->` " + parseType(v["ret"])
    if "extern" in v:
        s += f" (_{v['extern']} function_)"
    s += "\n"
    return s

for mk, m in modules.items():
    with open(f"out/{m}.md", "w") as f:
        f.write(f"# [{m}]({m}.md) module\n")
        f.write(f"Source code: [{m.split('.')[-1]}.seq]({j[mk]['path']})\n\n")
        if 'doc' in j[mk]:
            f.write(parseDocstr(j[mk]['doc']) + "\n")

        f.write(f"## Members:\n")
        for i in j[mk]['children']:
            v = j[i]

            if v['kind'] == 'class' and v['type'] == 'extension':
                v['name'] = j[v['parent']]['name']
            if v['name'].startswith('_'):
                continue

            f.write(f'### <a name="{i}"></a>')
            if v['kind'] == 'class':
                f.write(f'_{v["type"]}_ ')
            if v['kind'] == 'class' and v['type'] == 'extension':
                f.write(f'**`{getLink(v["parent"])}`**')
            else:
                f.write(f'{m}.**`{v["name"]}`**')
            if v['kind'] == 'function':
                f.write(parseFunction(v))
            if 'generics' in v and v['generics']:
                f.write(f"`[{', '.join(v['generics'])}]`")
            f.write("\n")
            if v['kind'] == 'function' and 'attrs' in v and v['attrs']:
                f.write("**Attributes:**" + ', '.join(f'`{x}`' for x in v['attrs']))
            if 'doc' in v:
                f.write(parseDocstr(v['doc']) + "\n")
            f.write("\n")

            if v['kind'] == 'class':
                if 'args' in v and any(c['name'][0] != '_' for c in v['args']):
                    f.write('#### Arguments:\n')
                    for c in v['args']:
                        if c['name'][0] == '_':
                            continue
                        f.write(f'- **`{c["name"]} : `**')
                        f.write(parseType(c["type"]) + "\n")
                        if 'doc' in c:
                            f.write(parseDocstr(c['doc'], 1) + "\n")
                        f.write("\n")

                mt = [c for c in v['members'] if j[c]['kind'] == 'function']

                props = [c for c in mt if 'property' in j[c].get('attrs', [])]
                if props:
                    f.write('#### Properties:\n')
                    for c in props:
                        v = j[c]
                        f.write(f'- <a name="{c}"></a> **`{v["name"]}`**')
                        if 'ret' in v:
                            f.write('` : `' + parseType(v['ret']))
                        if 'doc' in v:
                            f.write(parseDocstr(v['doc'], 1) + "\n")
                        f.write("\n")

                magics = [c for c in mt if len(j[c]['name']) > 4 and j[c]['name'].startswith('__') and j[c]['name'].endswith('__')]
                if magics:
                    f.write('#### Magic methods:\n')
                    for c in magics:
                        v = j[c]
                        f.write(f'- <a name="{c}"></a> **`{v["name"]}`**')
                        f.write(parseFunction(v) + "\n")
                        if 'doc' in v:
                            f.write(parseDocstr(v['doc'], 1) + "\n")
                        f.write("\n")
                methods = [c for c in mt if j[c]['name'][0] != '_' and c not in props]
                if methods:
                    f.write('#### Methods:\n')
                    for c in methods:
                        v = j[c]
                        f.write(f'- <a name="{c}"></a> **`{v["name"]}`**')
                        f.write(parseFunction(v) + "\n")
                        if 'doc' in v:
                            f.write(parseDocstr(v['doc'], 1) + "\n")
                        f.write("\n")
            f.write("\n")
