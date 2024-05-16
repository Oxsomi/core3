# Parser / Symbol tree

To keep track of symbols, a tree has to be created that contains all symbols.

![symbol_tree](symbol_tree.png)

Represents the following code:

```C
namespace T {
    struct Y {
        F32 Z;
    };
}

namespace T2 {
    static const F32 W = 1;
    F32 test() {
        F32 Z = 123;
        return z;
    }
}
```

Each symbol has a trail that represents on how to get there, this trail is a U64 split up as follows:

```c
U64 layer0 : 17;	//Generally things such as C/HLSL/GLSL symbols, can be a lot
U64 layer1 : 15;	//When namespaces are used, less common, but still common
U64 layer2 : 12;	//Double namespaces		
U64 layer3 : 9;		//Deeply nested templates
U64 layer4 : 4;
U64 layer5 : 4;
U64 layer6 : 3;
```

This means that we only support up to 7 levels of recursion of symbols and each level gets less symbols it can link to.