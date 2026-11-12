#include <random>
#include <string>
#include "titan_cpplib/ds/persistent_wbtree.cpp"
using namespace std;
using Tree = titan23::PersistentWBTree<int, long long>;
int main() {
    for (int seed : {1,23,47}) {
        Tree::ma.reset(); mt19937 rng(seed);
        vector<Tree> ts(1); vector<vector<int>> vs(1);
        for (int step=0;step<6000;++step) {
            size_t i=rng()%ts.size(),j=rng()%ts.size(); auto t=ts[i]; auto a=vs[i];
            int op=rng()%7;
            if(op==0 && a.size()+vs[j].size()<250) {
                t=t.merge(ts[j]);a.insert(a.end(),vs[j].begin(),vs[j].end());
            } else if(op==1) {
                size_t cap=rng()%100;t=t.merge_prefix(ts[j],cap);
                a.insert(a.end(),vs[j].begin(),vs[j].end());if(a.size()>cap)a.resize(cap);
            } else if(op==2 && a.size()<250) {
                size_t k=rng()%(a.size()+1);int v=rng()%1000;t=t.insert(k,v);a.insert(a.begin()+k,v);
            } else if(op==3 && !a.empty()) {
                size_t k=rng()%a.size();auto p=t.pop(k);assert(p.second==a[k]);t=p.first;a.erase(a.begin()+k);
            } else {
                size_t k=rng()%(a.size()+1);auto p=t.split(k);
                assert(p.first.tovector()==vector<int>(a.begin(),a.begin()+k));
                assert(p.second.tovector()==vector<int>(a.begin()+k,a.end()));
                assert(t.split_left(k).tovector()==p.first.tovector());
                assert(t.split_right(k).tovector()==p.second.tovector());
                t=p.first.merge(p.second);
            }
            t.check();assert(t.tovector()==a);assert(t.len()==(long long)a.size());
            for(size_t k=0;k<a.size();++k)assert(t.get(k)==a[k]);
            assert(ts[i].tovector()==vs[i]);assert(ts[j].tovector()==vs[j]);
            ts.push_back(t);vs.push_back(a);
        }
        for(size_t i=0;i<ts.size();++i)assert(ts[i].tovector()==vs[i]);
    }
    Tree t(vector<int>{0,1});
    for(int i=0;i<58;++i)t=t.merge(t);
    auto original=t;
    for(int i=0;i<200;++i) {
        auto a=t.split_left(t.len()-i), b=t.split_right(i);
        for(long long k:{0LL,17LL,a.len()-1})assert(a.get(k)==k%2);
        for(long long k:{0LL,17LL,b.len()-1})assert(b.get(k)==(k+i)%2);
        t=t.merge_prefix(original,1000000000000000000LL);
    }
    assert(original.len()==(1LL<<59));
    using U=titan23::PersistentWBTree<char,uint64_t>;
    U u(vector<char>{'x'});for(int i=0;i<63;++i)u=u.merge(u);
    assert(u.len()==(uint64_t(1)<<63));assert(u.get(u.len()-1)=='x');
    auto z=u.merge_prefix(u,UINT64_MAX);assert(z.len()==UINT64_MAX);assert(z.get(UINT64_MAX-1)=='x');
    using S=titan23::PersistentWBTree<string,long long>;
    S s(vector<string>{"a","bb","ccc"});auto v=s.merge(s);assert(v.get(4)=="bb");
    S::rebuild(v);assert(v.tovector()==vector<string>({"a","bb","ccc","a","bb","ccc"}));
    cout<<"persistent_wbtree_leaf: OK\n";
}
