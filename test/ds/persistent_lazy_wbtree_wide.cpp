#include <cassert>
#include <cstdint>
#include <iostream>
#ifndef PERSISTENT_LAZY_WBTREE_HEADER
#define PERSISTENT_LAZY_WBTREE_HEADER "titan_cpplib/ds/persistent_lazy_wbtree.cpp"
#endif
#include PERSISTENT_LAZY_WBTREE_HEADER
using namespace std;
constexpr long long mod=998244353;
struct S { long long sum; uint64_t count; };
struct F { long long a,b; bool operator!=(F v) const { return a!=v.a || b!=v.b; } };
S op(S x,S y){return {(x.sum+y.sum)%mod,x.count+y.count};}
S mapping(F f,S x){return {(f.a*x.sum+f.b*static_cast<long long>(x.count%mod))%mod,x.count};}
F composition(F f,F g){return {f.a*g.a%mod,(f.a*g.b+f.b)%mod};}
S e(){return {0,0};} F id(){return {1,0};}
template<class Size> void test(){
 using Tree=titan23::PersistentLazyWBTree<Size,S,F,op,mapping,composition,e,id>;
 Tree a(vector<S>{{0,1},{1,1}});
 for(int i=0;i<59;++i)a=a.merge(a);
 a=a.split_left(Size(1000000000000000000LL));
 auto b=a.apply(7,100000000000000003LL,{2,3});
 auto c=b.reverse(0,b.len());
 for(Size k:{Size(0),Size(6),Size(7),Size(99999999999999999LL),Size(100000000000000003LL),Size(999999999999999999LL)}){
  long long v=k%2;
  assert(a.get(k).sum==v);
  if(k>=7 && k<100000000000000003LL)v=2*v+3;
  assert(b.get(k).sum==v);
  assert(c.get(c.len()-1-k).sum==v);
 }
 auto before=Tree::ma.ptr;
 assert(a.prod(0,a.len()).sum==static_cast<long long>((uint64_t(a.len())/2)%mod));
 assert(Tree::ma.ptr==before);
 auto p=c.split(123456789012345LL);auto d=p.first.merge(p.second);
 for(Size k:{Size(0),Size(123456789012345LL),Size(999999999999999999LL)})assert(d.get(k).sum==c.get(k).sum);
 auto changed=d.set(12345,{8,1});assert(changed.get(12345).sum==8);assert(d.get(12345).sum==c.get(12345).sum);
 cout<<"wide lazy: "<<sizeof(Size)<<" bytes, slot="<<sizeof(typename Tree::MemoryAllocator::Slot)<<" OK\n";
}
int main(){test<long long>();test<uint64_t>();}
