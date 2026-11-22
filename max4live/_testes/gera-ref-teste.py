# música de teste: house 124 BPM com estrutura conhecida (gabarito no fim)
import numpy as np, sys
sr=44100; bpm=124.0; spb=60/bpm; rng=np.random.default_rng(1)
partes=[("INTRO",16,"kh"),("DROP",32,"kbhp"),("BREAK",16,"p"),("BUILD",8,"prs"),("DROP",32,"kbhp"),("GROOVE",8,"khp"),("DROP",16,"kbhp"),("OUTRO",16,"kh")]
lead=float(sys.argv[2]) if len(sys.argv)>2 else 0.37  # segundos de silêncio antes do 1º compasso
nb=sum(p[1] for p in partes); N=int((nb*4*spb+lead+1)*sr); y=np.zeros(N)
def kick():
    t=np.arange(int(0.25*sr))/sr; f=50+120*np.exp(-t*30); return np.sin(2*np.pi*np.cumsum(f)/sr)*np.exp(-t*9)
def hat():
    t=np.arange(int(0.05*sr))/sr; n=rng.standard_normal(len(t)); n=np.diff(n,prepend=0); return 0.25*n*np.exp(-t*80)
def bass():
    t=np.arange(int(spb/2*0.9*sr))/sr; return 0.5*np.sin(2*np.pi*55*t)*np.minimum(1,t*200)*np.exp(-t*3)
K,H,BS=kick(),hat(),bass()
def put(x,pos):
    i=int(pos*sr); j=min(N,i+len(x)); y[i:j]+=x[:j-i]
bar0=0
for nome,n,el in partes:
    for b in range(n):
        t0=lead+(bar0+b)*4*spb
        for bt in range(4):
            tb=t0+bt*spb
            if 'k' in el: put(K,tb)
            if 'h' in el: put(H,tb+spb/2)
            if 'b' in el: put(BS,tb+spb/2)
            if 's' in el and b>=4: 
                for q in range(4): put(0.3*H*3,tb+q*spb/4)
        if 'p' in el:
            t=np.arange(int(4*spb*sr))/sr; put(0.12*(np.sin(2*np.pi*220*t)+np.sin(2*np.pi*277*t)+np.sin(2*np.pi*330*t)),t0)
        if 'r' in el:
            t=np.arange(int(4*spb*sr))/sr; nz=rng.standard_normal(len(t))*0.05*((b*4*spb+t)/(n*4*spb)); put(np.diff(nz,prepend=0),t0)
    bar0+=n
y/=np.max(np.abs(y))*1.1
st=np.stack([y,y],1).astype('<f4'); st.tofile(sys.argv[1])
print(' '.join(f"{p[0]}:{p[1]}" for p in partes))
