//
// Created by levn on 20/12/18.
//

int add(int a, int b){
    return a + b;
}

int sub(int a, int b){
    return a - b;
}

int use(int (*fn)(int, int)) {
    int res = fn(10, 20);
    return res;
}

int main(){
    int x = use(add);
    int y = use(sub);
//    printf("%d %d\n", x, y);
    return x+y;
}