#include <iostream>
#include <vector>
#ifndef BINARY_CAT_HEADER
#define BINARY_CAT_HEADER "titan_cpplib/ds/persistent_wbtree.cpp"
#endif
#include BINARY_CAT_HEADER
using namespace std;
int main() {
    ios::sync_with_stdio(false);cin.tie(nullptr);
    using Tree=titan23::PersistentWBTree<char,long long>;
    int q;cin>>q;vector<Tree> t(q+2);
    t[0]=Tree(vector<char>{'0'});t[1]=Tree(vector<char>{'1'});
    constexpr long long limit=1000000000000000000LL;
    for(int i=0;i<q;++i) {
        int l,r;long long x;cin>>l>>r>>x;
#ifdef BINARY_CAT_SEPARATE_PREFIX
        t[i+2]=t[l].merge(t[r]);
        if(t[i+2].len()>limit)t[i+2]=t[i+2].split_left(limit);
#else
        t[i+2]=t[l].merge_prefix(t[r],limit);
#endif
        cout<<t[i+2].get(x-1)<<'\n';
    }
#ifndef BINARY_CAT_OLD
    cerr<<"nodes="<<Tree::ma.ptr-1<<" leaves="<<Tree::ma.leaf_ptr
             <<" node_bytes="<<sizeof(typename Tree::MemoryAllocator::Node)<<'\n';
#endif
}
