// RUN: %clang --target=patmos %s -o %t -Wl,--bogus-patmos-lld-flag 2>&1 | \
// RUN: FileCheck %s
// END.
///////////////////////////////////////////////////////////////////////////////////////////////////
//
// Tests that patmos lld is searched for on the path by default.
// We test this by providing an illegal flag to it, and ensuring it fails with a message identifying
// as 'ld.lld'
//
///////////////////////////////////////////////////////////////////////////////////////////////////
int main() { }

// CHECK: ld.lld: error: unknown argument '--bogus-patmos-lld-flag'
// CHECK: error: Link final executable command failed