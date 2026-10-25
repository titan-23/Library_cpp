// g++ -std=c++17 -O2 -I. test/ds/benchmark/candidates/wbt_indexed_bitmap/reference_stability_test.cpp -o /tmp/wbt_indexed_test
#include <cassert>
#include <string>
#include <vector>
#include "wb_tree_blocked.cpp"
int main(){
 titan23::WBTree<std::string> tree(std::vector<std::string>{"saved"});
 auto handle=tree.root;
 auto* address=handle.operator->();
 std::string& reference=tree[0];
 for(int i=0;i<10000;++i) tree.emplace_back(std::to_string(i));
 assert(handle.operator->()==address);
 assert(&tree[0]==&reference && reference=="saved" && address->key=="saved");
 reference="changed";
 assert(tree[0]=="changed");
 tree.isok();
}
