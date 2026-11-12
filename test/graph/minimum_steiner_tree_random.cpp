#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <queue>
#include <random>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>
#include "titan_cpplib/graph/minimum_steiner_tree.cpp"
using namespace std;
using namespace titan23;
using Weight=long long;
using Graph=vector<vector<pair<int,Weight>>>;
const Weight INF=(1LL<<60);
void edge(Graph& g,int u,int v,Weight w) {g[u].push_back({v,w});g[v].push_back({u,w});}
vector<tuple<Weight,int,int>> edges(const Graph& g) {
    map<pair<int,int>,Weight> w;
    for(int u=0;u<(int)g.size();++u)for(auto [v,c]:g[u])if(u<v) {
        auto [it,inserted]=w.emplace(pair(u,v),c);if(!inserted)it->second=min(it->second,c);
    }
    vector<tuple<Weight,int,int>> es;for(auto [uv,c]:w)es.emplace_back(c,uv.first,uv.second);sort(es.begin(),es.end());return es;
}
Weight optimum(const Graph& g,const vector<int>& ts) {
    int required=0;for(int t:ts)required|=1<<t;
    if(__builtin_popcount((unsigned)required)<=1)return 0;
    Weight best=INF;auto es=edges(g);int n=g.size();
    for(int mask=required;mask<(1<<n);++mask)if((mask&required)==required) {
        vector<int> p(n);iota(p.begin(),p.end(),0);auto root=[&](int v){while(p[v]!=v)v=p[v];return v;};
        int cnt=0;Weight cost=0;
        for(auto [w,u,v]:es)if((mask>>u&1)&&(mask>>v&1)) {int a=root(u),b=root(v);if(a!=b){p[a]=b;cost+=w;++cnt;}}
        if(cnt==__builtin_popcount((unsigned)mask)-1)best=min(best,cost);
    }
    return best;
}
void verify(const Graph& g,const vector<int>& ts,const SteinerTreeResult<Weight>& r,Weight opt,bool bounded=true) {
    assert(r.feasible==(opt!=INF));if(!r.feasible){assert(r.edges.empty());return;}
    int n=g.size();vector<int> p(n),degree(n),terminal(n);iota(p.begin(),p.end(),0);
    auto root=[&](int v){while(p[v]!=v)v=p[v];return v;};
    map<pair<int,int>,Weight> weights;for(auto [w,u,v]:edges(g))weights[{u,v}]=w;
    Weight cost=0;
    for(auto [u,v]:r.edges) {
        assert(u<v&&weights.count({u,v}));cost+=weights.at({u,v});int a=root(u),b=root(v);assert(a!=b);p[a]=b;++degree[u];++degree[v];
    }
    for(int t:ts){terminal[t]=1;assert(root(t)==root(ts[0]));}
    for(int v=0;v<n;++v) {if(degree[v])assert(!ts.empty()&&root(v)==root(ts[0]));if(degree[v]==1)assert(terminal[v]);}
    assert(cost==r.total_cost&&cost>=opt);
    int k=accumulate(terminal.begin(),terminal.end(),0);
    if(k>1) {if(bounded)assert(cost*k<=2LL*(k-1)*opt);}
    else assert(r.edges.empty()&&cost==0);
}
template<class E,class F>void throws(F f){bool caught=false;try{f();}catch(const E&){caught=true;}assert(caught);}
// Independent reference: restart multi-source Dijkstra from the entire tree
// after each attachment, instead of retaining or caching any distances.
Weight reference_grow(const Graph& g,const vector<int>& terminals) {
    int n=g.size();vector<int> tree(n),required(n);for(int t:terminals)required[t]=1;
    if(terminals.empty())return 0;
    tree[terminals.back()]=1;Weight total=0;
    while(true) {
        bool done=true;for(int t:terminals)if(!tree[t])done=false;if(done)return total;
        vector<Weight>d(n,INF),pw(n);vector<int>p(n,-1);
        priority_queue<pair<Weight,int>,vector<pair<Weight,int>>,greater<pair<Weight,int>>> q;
        for(int v=0;v<n;++v)if(tree[v]){d[v]=0;q.emplace(0,v);}
        int target=-1;
        while(!q.empty()) {
            auto[c,v]=q.top();q.pop();if(c!=d[v])continue;
            if(required[v]&&!tree[v]){target=v;break;}
            for(auto[u,w]:g[v])if(c+w<d[u]){d[u]=c+w;p[u]=v;pw[u]=w;q.emplace(d[u],u);}
        }
        if(target<0)return INF;
        for(int v=target;!tree[v];v=p[v]){tree[v]=1;total+=pw[v];}
    }
}
int main(){
    mt19937 rng(23);
    for(int test=0;test<100;++test) {
        Graph g(8);int id=0;
        for(int u=0;u<8;++u)for(int v=u+1;v<8;++v)if(v==u+1||rng()%2)edge(g,u,v,1LL<<id++);
        vector<int> terminals;for(int v=0;v<8;++v)if(rng()%2)terminals.push_back(v);
        MinimumSteinerTree<Weight> s(g,INF);auto expected=reference_grow(g,terminals);
        assert(s.solve(terminals).total_cost==expected);
        assert(s.solve(terminals,SteinerTreeMethod::incremental_prim).total_cost==expected);
        if(!terminals.empty()) {
            Weight best=INF;for(int i=0;i<(int)terminals.size();++i){auto order=terminals;swap(order[i],order.back());best=min(best,s.solve(order).total_cost);}
            auto es=s.build_prim_fullsearch(terminals);Weight c=0;map<pair<int,int>,Weight>w;for(auto[a,u,v]:edges(g))w[{u,v}]=a;for(auto e:es)c+=w.at(e);assert(c==best);
        }
    }
    for(int test=0;test<500;++test){
        int n=1+rng()%9;Graph g(n);
        for(int u=0;u<n;++u)for(int v=u+1;v<n;++v)if(rng()%3){edge(g,u,v,rng()%15);if(rng()%5==0)edge(g,u,v,rng()%15);}
        MinimumSteinerTree<Weight> solver(g,INF);
        for(int q=0;q<4;++q){
            vector<int> ts;for(int v=0;v<n;++v)if(rng()%2)ts.push_back(v);if(!ts.empty()&&q%2==0)ts.push_back(ts[0]);shuffle(ts.begin(),ts.end(),rng);
            Weight opt=optimum(g,ts);
            for(auto method:{SteinerTreeMethod::cached_prim,SteinerTreeMethod::incremental_prim,SteinerTreeMethod::mehlhorn,SteinerTreeMethod::mst_prune}){
                auto r=solver.solve(ts,method);verify(g,ts,r,opt,method!=SteinerTreeMethod::mst_prune);auto repeat=solver.solve(ts,method);assert(r.edges==repeat.edges&&r.total_cost==repeat.total_cost);
            }
            auto best=solver.solve_best(ts,3,23);verify(g,ts,best,opt);
            auto a=solver.solve(ts,SteinerTreeMethod::incremental_prim),b=solver.solve(ts,SteinerTreeMethod::mehlhorn);
            if(best.feasible)assert(best.total_cost<=a.total_cost&&best.total_cost<=b.total_cost);
            auto cachedbest=solver.solve_best(ts,3,23,SteinerTreeMethod::cached_prim);verify(g,ts,cachedbest,opt);
            if(opt==INF){throws<runtime_error>([&]{solver.build_prim(ts);});throws<runtime_error>([&]{solver.build_prim_fullsearch(ts);});}
            else {
                auto es=solver.build_prim_fullsearch(ts);Weight cost=0;auto all=edges(g);map<pair<int,int>,Weight>w;for(auto [c,u,v]:all)w[{u,v}]=c;for(auto e:es)cost+=w.at(e);verify(g,ts,{es,cost,true},opt);
            }
        }
        solver.clear_cache();assert(solver.cached_source_count()==0);
    }
    Graph g(3);edge(g,0,1,4);edge(g,1,2,5);MinimumSteinerTree<Weight> solver(g,INF);
    assert(solver.cached_source_count()==0);solver.solve({0,2});assert(solver.cached_source_count()==1);
    solver.solve({0,2});assert(solver.cached_source_count()==1);
    solver.clear_cache();assert(solver.cached_source_count()==0);assert(solver.solve({0,2}).total_cost==9);
    throws<invalid_argument>([&]{solver.solve({-1});});throws<invalid_argument>([&]{solver.solve({3});});
    throws<invalid_argument>([&]{solver.solve_best({0,2},-1);});
    Graph bad(2);bad[0].push_back({1,1});throws<invalid_argument>([&]{MinimumSteinerTree<Weight>s(bad,INF);});
    edge(bad,0,1,-1);throws<invalid_argument>([&]{MinimumSteinerTree<Weight>s(bad,INF);});
    throws<invalid_argument>([&]{MinimumSteinerTree<Weight>s(g,0);});
    Graph big(4);for(int i=1;i<4;++i)edge(big,0,i,49);MinimumSteinerTree<Weight> limited(big,100);
    for(auto m:{SteinerTreeMethod::cached_prim,SteinerTreeMethod::incremental_prim,SteinerTreeMethod::mehlhorn})throws<overflow_error>([&]{limited.solve({1,2,3},m);});
    MinimumSteinerTree<Weight> empty;assert(empty.solve({}).feasible);
    Graph chain(8);for(int i=1;i<8;++i)edge(chain,i-1,i,1);edge(chain,0,7,2);
    MinimumSteinerTree<Weight> chain_solver(chain,INF);
    auto pruned=chain_solver.solve({0,7},SteinerTreeMethod::mst_prune);verify(chain,{0,7},pruned,2,false);assert(pruned.total_cost==7);
    assert(chain_solver.solve({0,7}).total_cost==2);
    chain_solver.clear_cache();assert(chain_solver.solve({0,7},SteinerTreeMethod::mst_prune).total_cost==7);
    vector<vector<pair<int,double>>> gf={{{1,0.25}},{{0,0.25},{2,0.5}},{{1,0.5}}};
    MinimumSteinerTree<double> sf(gf,1e100);assert(sf.solve({0,2}).total_cost==.75);assert(sf.solve_best({0,2}).total_cost==.75);
    gf[0][0].second=numeric_limits<double>::quiet_NaN();throws<invalid_argument>([&]{MinimumSteinerTree<double>s(gf,1e100);});
    vector<vector<pair<int,unsigned long long>>> gu(3);auto w=numeric_limits<unsigned long long>::max()/4;
    gu[0].push_back({1,w});gu[1]={{0,w},{2,w}};gu[2].push_back({1,w});
    MinimumSteinerTree<unsigned long long> su(gu,numeric_limits<unsigned long long>::max());assert(su.solve({0,2}).total_cost==2*w);
    cerr<<"All Steiner correctness checks passed.\n";
}
