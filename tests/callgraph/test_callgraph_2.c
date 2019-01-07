//
// Created by levn on 20/12/18.
//
#include <stdio.h>
struct foo{
    int* n;
    int (*fn)(int, int);
};

int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}


int use_2(struct foo* A) {
    int n_ = *A->n;
    printf("%d\n",n_);    
    int res = A->fn(100, 200);
    return res;
}

int main(){
    int *p;
    *p = 15;
    struct foo A = {p, add};
    int bar = use_2(&A);
    printf("%d\n", bar);
    return bar;
}
