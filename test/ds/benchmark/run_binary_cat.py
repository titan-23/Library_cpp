#!/usr/bin/env python3
import argparse,hashlib,json,random,subprocess,time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3]
WORK=Path('/tmp/binary-cat-wbtree')
CAP=10**18

def sha(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  for b in iter(lambda:f.read(1048576),b''):h.update(b)
 return h.hexdigest()

def generate(q):
 WORK.mkdir(exist_ok=True)
 info={}
 for kind in ['uniform','recent','prepend']:
  rng=random.Random(23);sizes=[1,1];roots=[0,1];nodes=[None,None];expected=hashlib.sha256()
  path=WORK/(kind+'.in')
  with path.open('w') as f:
   f.write(str(q)+'\n')
   for i in range(q):
    if kind=='prepend':
     l,r=(i+1,i+1) if i<60 else (i%2,i+1)
    elif kind=='recent':
     l=rng.randrange(max(0,i-1000),i+2);r=rng.randrange(max(0,i-1000),i+2)
    else:l=rng.randrange(i+2);r=rng.randrange(i+2)
    n=min(CAP,sizes[l]+sizes[r]);x=rng.randrange(n)+1
    if kind=='prepend' and i>=60 and i%2==0:x=rng.randrange(min(i-59,1000))+1
    f.write(f'{l} {r} {x}\n')
    if kind=='prepend':
     answer=1 if i<60 else ((i-(x-1))%2 if x<=i-59 else 1)
    else:
     if sizes[l]>=CAP:root=roots[l]
     else:root=len(nodes);nodes.append((roots[l],roots[r],sizes[l]))
     roots.append(root);k=x-1
     while root>=2:
      a,b,s=nodes[root]
      if k<s:root=a
      else:k-=s;root=b
     answer=root
    expected.update((str(answer)+'\n').encode());sizes.append(n)
  info[kind]={'input_sha256':sha(path),'expected_sha256':expected.hexdigest(),'q':q}
 (WORK/'inputs.json').write_text(json.dumps(info,indent=2))

def run(repeats):
 info=json.loads((WORK/'inputs.json').read_text())
 sources={'x':ROOT/'x.cpp','y':ROOT/'y.cpp','leaf_separate':ROOT/'test/ds/benchmark/binary_cat_wbtree.cpp','leaf':ROOT/'test/ds/benchmark/binary_cat_wbtree.cpp'}
 result={'compiler':subprocess.check_output(['g++','--version'],text=True).splitlines()[0],'seed':23,'runner_sha256':sha(Path(__file__)),'flags':['-std=c++17','-O3','-DNDEBUG'],'cpu':24,'address_space_limit_mib':1536,'inputs':info,'sources':{k:sha(v) for k,v in sources.items()},'leaf_header_sha256':sha(ROOT/'titan_cpplib/ds/persistent_wbtree.cpp'),'runs':[]}
 for name,source in sources.items():
  cmd=['g++',*result['flags'],'-I',str(ROOT),str(source),'-o',str(WORK/name)]
  if name=='leaf_separate':cmd+=['-DBINARY_CAT_SEPARATE_PREFIX']
  subprocess.run(cmd,check=True)
 for kind in info:
  for name in sources:
   for repeat in range(repeats):
    out=WORK/'out';err=WORK/'err';metrics=WORK/'metrics'
    with (WORK/(kind+'.in')).open('rb') as inp,out.open('wb') as dst,err.open('wb') as log:
     cmd=['/usr/bin/time','-f','%e %M','-o',str(metrics),'timeout','40','taskset','-c','24','prlimit','--as='+str(1536*2**20),'--core=0','--',str(WORK/name)]
     p=subprocess.run(cmd,stdin=inp,stdout=dst,stderr=log)
    last=metrics.read_text().strip().splitlines()[-1].split()
    row={'workload':kind,'variant':name,'repeat':repeat+1,'exit_code':p.returncode,'seconds':float(last[0]),'peak_rss_mib':int(last[1])/1024,'correct':p.returncode==0 and sha(out)==info[kind]['expected_sha256'],'stderr_tail':err.read_text()[-300:]}
    result['runs'].append(row);print(row,flush=True)
    (ROOT/'test/ds/benchmark/binary_cat_comparison.json').write_text(json.dumps(result,indent=2)+'\n')
    if p.returncode:break
    if not row['correct']:raise RuntimeError('output mismatch')

if __name__=='__main__':
 p=argparse.ArgumentParser();p.add_argument('--generate',action='store_true');p.add_argument('--q',type=int,default=500000);p.add_argument('--repeats',type=int,default=1);a=p.parse_args()
 if a.generate:generate(a.q)
 else:run(a.repeats)
