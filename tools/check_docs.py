#!/usr/bin/env python3
"""Lumora documentation consistency check.
Usage: python3 tools/check_docs.py [repo_root]
Checks: section cross-references resolve, doc file references exist or are planned,
markdown links resolve, effort totals match the table, counts match tables,
no superseded decisions or version tags outside history files, no private details
(message ids, timestamps, Arabic quotes) in repo files, decision log D1-D31 complete,
master length, PROJECT_STATE template headings. Exit code 1 on any failure."""
import re,sys,os,glob
root=sys.argv[1] if len(sys.argv)>1 else os.path.join(os.path.dirname(os.path.abspath(__file__)),'..')
root=os.path.abspath(root)
def rd(p): return open(p,encoding='utf-8').read()
master=os.path.join(root,'LUMORA_MASTER.md')
docs=sorted(glob.glob(os.path.join(root,'docs','*.md')))
HIST={'docs/review-history.md','CHANGELOG.md'}
allf=[master]+docs+[os.path.join(root,x) for x in ('PROJECT_STATE.md','CHANGELOG.md') if os.path.exists(os.path.join(root,x))]
rel=lambda p: os.path.relpath(p,root).replace(os.sep,'/')
fails=[];notes=[]
def fail(m): fails.append(m)
text={rel(p):rd(p) for p in allf if os.path.exists(p)}
normative={k:v for k,v in text.items() if k not in HIST}
# 1 section headings
defs={}
for f,t in text.items():
    for m in re.finditer(r'^#{2,4} (\d+A?(?:\.\d+)*)\.? ',t,re.M):
        defs.setdefault(m.group(1),[]).append(f)
for k,v in defs.items():
    if len(set(v))>1: fail(f'section {k} defined in several files: {sorted(set(v))}')
    if len(v)>1 and len(set(v))==1: fail(f'section {k} defined twice in {v[0]}')
HISTSEC={str(n) for n in range(22,32)}
for f,t in normative.items():
    t2=re.sub(r'```.*?```','',t,flags=re.S)
    if f=='LUMORA_MASTER.md': t2='\n'.join(l for l in t2.split('\n') if 'review-history' not in l)
    for m in re.finditer(r'(?:Sections?|§)\s*(\d+A?(?:\.\d+)*)((?:\s*(?:,|and|to)\s*\d+A?(?:\.\d+)*)*)',t2):
        refs=[m.group(1)]+re.findall(r'\d+A?(?:\.\d+)*',m.group(2) or '')
        for r in refs:
            if r not in defs: fail(f'{f}: reference to missing Section {r}')
            elif r.split('.')[0] in HISTSEC: fail(f'{f}: normative text points into history Section {r}')
# 2 doc file refs
planned=set(re.findall(r'`(docs/[\w.-]+\.md)`',re.search(r'## Documents map(.*?)(?=\n## )',text['LUMORA_MASTER.md'],re.S).group(1))) if re.search(r'## Documents map',text['LUMORA_MASTER.md']) else set()
exist={rel(p) for p in docs}|{'docs/PROJECT_STATE.md'}
for f,t in normative.items():
    for r in set(re.findall(r'docs/[\w./-]*[\w-]\.md',t)):
        if r not in exist and r not in planned: fail(f'{f}: references {r}, which neither exists nor is in the documents map')
    for m in re.finditer(r'\]\(([^)#]+\.md)(?:#[^)]*)?\)',t):
        tgt=os.path.normpath(os.path.join(os.path.dirname(os.path.join(root,f)),m.group(1)))
        if not os.path.exists(tgt): fail(f'{f}: broken link {m.group(1)}')
# 3 totals
ms=text.get('docs/milestones.md','')
rows=[(int(a),int(b)) for a,b in re.findall(r'^\| (?!\*\*Total)[^|]+ \| (\d+)-(\d+) \|$',ms,re.M)]
tot=re.search(r'\*\*Total \(serial\)\*\* \| \*\*(\d+)-(\d+)\*\*',ms)
if not rows or not tot: fail('effort table or total row not found')
else:
    lo,hi=sum(r[0] for r in rows),sum(r[1] for r in rows)
    if (lo,hi)!=(int(tot.group(1)),int(tot.group(2))): fail(f'effort total {tot.group(0)} but rows sum to {lo}-{hi}')
    mo=re.search(r'roughly (\d+)-(\d+) months',ms)
    if not mo or (round(lo/4.33),round(hi/4.33))!=(int(mo.group(1)),int(mo.group(2))): fail(f'months statement does not match {lo}-{hi} weeks')
    mt=text['LUMORA_MASTER.md']
    if f'{lo}-{hi} weeks' not in mt: fail(f'master does not state the {lo}-{hi} week total')
# milestone names in master table vs milestones.md
ids=re.findall(r'^- \*\*(M[0-9A-Za-z-]+):',ms,re.M)
for i in ids:
    if not re.search(r'^\| '+re.escape(i)+r' \|',text['LUMORA_MASTER.md'],re.M): fail(f'milestone {i} missing from master milestone table')
# decision log
mt=text['LUMORA_MASTER.md']
got=re.findall(r'^\| (D\d+) \|',mt,re.M)
if got!=[f'D{i}' for i in range(1,32)]: fail(f'decision log rows are {got}')
# open questions count
oqs=re.findall(r'^\d+\. \*\*(OQ\d+)\b',mt,re.M) or re.findall(r'^- \*\*(OQ\d+)\b',mt,re.M) or re.findall(r'^\| (OQ\d+) \|',mt,re.M)
n=re.search(r'(\d+) open owner questions',mt)
if not oqs or not n or int(n.group(1))!=len(oqs): fail(f'open-question count statement {n and n.group(1)} vs {len(oqs)} listed')
for f,t in normative.items():
    for r in re.findall(r'\bOQ(\d+)\b',t):
        if f'OQ{r}' not in oqs: fail(f'{f}: references unknown OQ{r}')
# disposition counts
if 'docs/review-history.md' in text and 'CHANGELOG.md' in text:
    h=text['docs/review-history.md']
    h=h[h.index('## 31.'):]
    d=re.findall(r'^\| ([A-Z]\d+) \| [^|]+ \| (Accept|Modify|Reject) \|',h,re.M)
    c={k:sum(1 for _,x in d if x==k) for k in ('Accept','Modify','Reject')}
    cl=re.search(r'Accepted: (\d+)\. Modified: (\d+)\. Rejected: (\d+)\.',text['CHANGELOG.md'])
    if not cl or (int(cl.group(1)),int(cl.group(2)),int(cl.group(3)))!=(c['Accept'],c['Modify'],c['Reject']): fail(f'changelog counts {cl and cl.groups()} vs table {c}')
# 4 superseded / version tags / private
SUP=[r'Option B\b',r'22\.04\s+(?:as\s+)?(?:the\s+)?(?:build\s+)?baseline',r'G1-G6',r'G1 to G6',r'GitHub at the end',r'\bD21\b',r'no Windows runner',r'hands frames']
for f,t in normative.items():
    for i,l in enumerate(t.split('\n'),1):
        for p in SUP:
            if re.search(p,l):
                if f=='LUMORA_MASTER.md' and re.match(r'\| D\d+ \|',l): continue
                fail(f'{f}:{i}: superseded term /{p}/ outside history')
        if re.search(r'\bv1\.\d+\b(?!\.\d)',l) and not re.search(r'DXC v1\.\d',l): fail(f'{f}:{i}: inline version tag')
        if re.search(r'\b\d{7,9}\b',l) and re.search(r'message|Telegram',l,re.I): fail(f'{f}:{i}: message identifier')
        if re.search(r'\d\d:\d\d EET',l): fail(f'{f}:{i}: timestamp')
        if re.search('[\u0600-\u06FF]',l.replace('ابدأ','')): fail(f'{f}:{i}: Arabic text outside the start word')
for f in HIST:
    if f in text:
        for i,l in enumerate(text[f].split('\n'),1):
            if re.search(r'\d\d:\d\d EET',l) or re.search('[\u0600-\u06FF]',l.replace('ابدأ','')) or (re.search(r'\b\d{7,9}\b',l) and re.search(r'message|Telegram',l,re.I)): fail(f'{f}:{i}: private detail in history file')
# 5 master length, state template
ml=len(mt.split('\n'))
if ml>320: fail(f'master has {ml} lines (limit 320)')
notes.append(f'master lines: {ml}')
ps=text.get('docs/PROJECT_STATE.md','')
for h in ('last_completed_milestone','Current milestone','In flight','Known failures','Next five tasks','Where CI logs'):
    if h not in ps: fail(f'PROJECT_STATE.md missing "{h}"')
print('\n'.join(notes))
if fails:
    print(f'FAIL: {len(fails)} problem(s)'); print('\n'.join(fails)); sys.exit(1)
print('OK: all consistency checks passed')
