#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <numeric>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#define MinimumSteinerTree LegacyMinimumSteinerTree
#include "fixtures/minimum_steiner_tree_20261112.cpp"
#undef MinimumSteinerTree
#include "titan_cpplib/graph/minimum_steiner_tree.cpp"
using namespace std;
using namespace titan23;
using W=long long;
using G=vector<vector<pair<int,W>>>;
const W INF=1LL<<60;
using Clock=chrono::steady_clock;
double ms(Clock::time_point t){return chrono::duration<double,milli>(Clock::now()-t).count();}
struct Spec {string name;int n,m,k;};
G generate(const Spec& s,int seed){
    mt19937 rng(seed);G g(s.n);set<pair<int,int>> seen;
    auto add=[&](int u,int v){if(u>v)swap(u,v);if(u==v||!seen.emplace(u,v).second)return;W w=1+rng()%10000;g[u].push_back({v,w});g[v].push_back({u,w});};
    if(s.name.starts_with("grid")) {int side=int(sqrt(s.n));for(int v=0;v<s.n;++v){if(v%side)add(v,v-1);if(v>=side)add(v,v-side);}}
    else {for(int v=1;v<s.n;++v)add(v,rng()%v);while((int)seen.size()<s.m)add(rng()%s.n,rng()%s.n);}
    return g;
}
int main(){
    vector<Spec> specs; for(int k:{5,20,50,90}) {specs.push_back({"grid100_k"+to_string(k),100,180,k}); specs.push_back({"sparse100_k"+to_string(k),100,400,k}); specs.push_back({"dense100_k"+to_string(k),100,4000,k});}
    cout<<"case,seed,n,m,k,method,setup_ms,cold_ms,warmup_ms,warm_ms,warm_cost,first_cost,cache_rows\n"<<setprecision(14);
    for(auto spec:specs)for(int seed=0;seed<10;++seed){
        auto g=generate(spec,1000+seed);mt19937 rng(seed+234);vector<vector<int>> queries(20);vector<int> order(spec.n);iota(order.begin(),order.end(),0);
        for(auto& q:queries){shuffle(order.begin(),order.end(),rng);q.assign(order.begin(),order.begin()+spec.k);}
        map<pair<int,int>,W> weights;for(int u=0;u<spec.n;++u)for(auto[v,w]:g[u])if(u<v)weights[{u,v}]=w;
        auto cost=[&](const vector<pair<int,int>>& es,const vector<int>& terminals){
            vector<int> p(spec.n),deg(spec.n),term(spec.n);iota(p.begin(),p.end(),0);auto root=[&](int v){while(v!=p[v])v=p[v];return v;};W total=0;
            for(auto[u,v]:es){if(u>v)swap(u,v);if(!weights.count({u,v}))throw runtime_error("not edge");int a=root(u),b=root(v);if(a==b)throw runtime_error("cycle");p[a]=b;++deg[u];++deg[v];total+=weights[{u,v}];}
            for(int t:terminals){term[t]=1;if(root(t)!=root(terminals[0]))throw runtime_error("not spanning terminals");}
            for(int v=0;v<spec.n;++v)if(deg[v]==1&&!term[v])throw runtime_error("unpruned leaf");
            return total;
        };
        for(int z=0;z<9;++z){int method=(z+seed)%9;auto begin=Clock::now();
            unique_ptr<LegacyMinimumSteinerTree<W>> old;unique_ptr<MinimumSteinerTree<W>> now;
            if(method==0||method==6)old=make_unique<LegacyMinimumSteinerTree<W>>(g,INF);else now=make_unique<MinimumSteinerTree<W>>(g,INF);
            double setup=ms(begin);
            auto run=[&](const vector<int>& q){
                if(method==0)return old->build_prim(q);
                if(method==6)return old->build_prim_fullsearch(q);
                if(method==1)return now->build_prim(q);
                if(method==2)return now->build_prim_dijkstra(q);
                if(method==3)return now->build_mehlhorn(q);
                if(method==4)return now->solve_best(q,8,23).edges;
                if(method==7)return now->solve_best(q,8,23,SteinerTreeMethod::cached_prim).edges;
                if(method==8)return now->build_mst_prune(q);
                return now->build_prim_fullsearch(q);
            };
            begin=Clock::now();auto first=run(queries[0]);double cold=ms(begin);W firstcost=cost(first,queries[0]);
            begin=Clock::now();for(auto& q:queries)run(q);double warmup=ms(begin);
            W total=0;for(auto& q:queries)total+=cost(run(q),q);
            size_t checksum=0;int rounds=(method==5||method==6)?3:30;
            begin=Clock::now();for(int repeat=0;repeat<rounds;++repeat)for(auto& q:queries)checksum+=run(q).size();
            double warm=ms(begin)/rounds; if(checksum==0)throw runtime_error("empty benchmark");
            vector<string> names={"legacy","cached","incremental","mehlhorn","best8","fullsearch","legacy_fullsearch","best8_cached","mst_prune"};
            cout<<spec.name<<','<<seed<<','<<spec.n<<','<<weights.size()<<','<<spec.k<<','<<names[method]<<','<<setup<<','<<cold<<','<<warmup<<','<<warm/queries.size()<<','<<double(total)/queries.size()<<','<<firstcost<<','<<(now?now->cached_source_count():spec.n)<<'\n'<<flush;
        }
    }
    return 0;
}
