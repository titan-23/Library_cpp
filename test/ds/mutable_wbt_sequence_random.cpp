#include <algorithm>
#include <cassert>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <vector>
#include "titan_cpplib/ds/wb_tree.cpp"
#include "titan_cpplib/ds/wb_tree_seg.cpp"
using namespace std;

long long sum_op(long long a, long long b) { return a+b; }
long long sum_e() { return 0; }
string str_op(string a, string b) { return a+b; }
string str_e() { return {}; }
using Seg = titan23::WBSegTree<long long, sum_op, sum_e>;
using Str = titan23::WBSegTree<string, str_op, str_e>;

void verify(const titan23::WBTree<long long>& tree, const vector<long long>& v) {
    assert(tree.len()==int(v.size()));
    assert(tree.tovector()==v);
    tree.isok();
    for (int i=0;i<int(v.size());++i) {
        assert(tree[i]==v[i]);
    }
    if (tree.root) assert(tree.root->key==v[tree.root->left_size]);
}
void verify(const Seg& tree, const vector<long long>& v) {
    assert(tree.len()==int(v.size()));
    assert(tree.tovector()==v);
    tree.check();
    assert(tree.all_prod()==accumulate(v.begin(),v.end(),0LL));
    for (int i=0;i<int(v.size());++i) assert(tree.get(i)==v[i]);
    for (int l=0;l<=int(v.size());l+=max(1,int(v.size())/11)) {
        for (int r=l;r<=int(v.size());r+=max(1,int(v.size())/7))
            assert(tree.prod(l,r)==accumulate(v.begin()+l,v.begin()+r,0LL));
    }
    if (tree.root) {
        assert(tree.root->key==v[tree.root->left_size]);
        assert(tree.root->data==tree.all_prod());
    }
}

template<class Tree> void random_test(uint64_t seed) {
    mt19937_64 rng(seed);
    vector<long long> v(47);
    iota(v.begin(),v.end(),0LL);
    Tree tree=v; // Retain implicit construction from vector.
    for (int step=0;step<10000;++step) {
        const int n=v.size();
        int k=rng()%(n+1);
        switch(rng()%7) {
        case 0:
        case 1: if (n<300) { long long x=int(rng()%1000)-500; tree.insert(k,x); v.insert(v.begin()+k,x); } break;
        case 2:
        case 3: if(n) {k%=n; assert(tree.pop(k)==v[k]);v.erase(v.begin()+k);} break;
        case 4: if(n) {k%=n;long long x=int(rng()%1000)-500;tree.set(k,x);v[k]=x;} break;
        case 5: {
            auto parts=tree.split(k);
            assert(parts.first.tovector()==vector<long long>(v.begin(),v.begin()+k));
            assert(parts.second.tovector()==vector<long long>(v.begin()+k,v.end()));
            parts.first.merge(parts.second);
            tree=parts.first;
            break;
        }
        case 6: {
            auto parts=tree.split(k);
            parts.second.merge(parts.first);
            tree=parts.second;
            rotate(v.begin(),v.begin()+k,v.end());
            break;
        }
        }
        if (step%29==0) verify(tree,v);
    }
    verify(tree,v);
    while(!v.empty()) {assert(tree.pop(0)==v.front());v.erase(v.begin());}
    verify(tree,v);
    auto empty=tree.split(0); empty.first.merge(empty.second);
    verify(empty.first,v);
}

void basic_api_tests() {
    titan23::WBTree<long long> explicit_empty(vector<long long>{});
    Seg explicit_empty_seg(vector<long long>{});
    assert(explicit_empty.len()==0 && explicit_empty_seg.len()==0);
    titan23::WBTree<long long> tree;
    tree.build({1,2,3}); tree.emplace_back(4);
    assert(tree.pop(tree.len()-1)==4 && tree.pop(0)==1);
    verify(tree,{2,3});
    auto alias=tree;
    tree.set(0,99); assert(alias[0]==99);
    tree[tree.len()-1]=42;
    long long &element=tree[0]; element=100;
    verify(tree,{100,42});
    verify(alias,{100,42});
    tree.clear();
    verify(tree,{});
    verify(alias,{100,42}); // clear only detaches this handle, as before.
    // A split consumes the original structure; use only its returned handles.
    for(int k=0;k<=7;++k) {
        titan23::WBTree<long long> t(vector<long long>{0,1,2,3,4,5,6});
        auto parts=t.split(k);
        parts.first.isok(); parts.second.isok();
        parts.first.merge(parts.second);
        verify(parts.first,{0,1,2,3,4,5,6});
    }
    Seg seg(vector<long long>{1,2,3});
    auto seg_alias=seg; seg.set(1,10); assert(seg_alias.all_prod()==14);
    ostringstream out; const Seg empty; out<<empty; assert(out.str()=="[]");
}

void ordered_aggregate_test() {
    vector<string> values={"a","bb","c","ddd","e"};
    Str tree=values;
    mt19937 rng(23);
    for(int step=0;step<1500;++step) {
        int n=values.size(),k=rng()%(n+1);
        if(step%4==0&&n<100) {string x(1,char('a'+rng()%26));tree.insert(k,x);values.insert(values.begin()+k,x);}
        else if(step%4==1&&n) {k%=n;assert(tree.pop(k)==values[k]);values.erase(values.begin()+k);}
        else if(step%4==2&&n) {k%=n;tree.set(k,"xy");values[k]="xy";}
        else {auto parts=tree.split(k);parts.first.merge(parts.second);tree=parts.first;}
        tree.check(); assert(tree.tovector()==values);
        for(int trial=0;trial<5;++trial) {
            int l=rng()%(values.size()+1),r=rng()%(values.size()+1);if(l>r)swap(l,r);
            string expected;for(int i=l;i<r;++i)expected+=values[i];
            assert(tree.prod(l,r)==expected);
        }
    }
}
int main() {
    basic_api_tests();
    for(uint64_t seed:{1,23,47,20261026}) {
        random_test<titan23::WBTree<long long>>(seed);
        random_test<Seg>(seed);
    }
    ordered_aggregate_test();
    cout<<"mutable WBTree/WBSegTree: OK\n";
}
