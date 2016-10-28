In order to build cntk_to_caffe model converter, you have to change CaffeRepositoryRoot and CntkRepositoryRoot variables
in src/common_settings.props to point to Caffe and CNTK repositories which contain source code and built solutions. If
you are building debug version of converter, CNTK repository (default build folder) should contain debug_cpuonly flavor
of CNTK and Caffe should contain built debug flavor of Caffe. For release version, similarly.

Conversion algorithm (omitting details for clarity)

The idea:
1. Convert CNTK tree to postfix notation
2. Repeat until all nodes in postfix order are visited
   2.1  Push next CNTK node from postfix notation to stack
   2.2  Find reduce rule for top of the stack. Reduce rule should convert CNTK
        subtree from the top of the stack to layer representation (Caffe layer).
   2.3  If rule exists apply it and go to 2.2
3. Serialize converted layer representations

Example:
    Let postfix notation of CNTK tree be: (Input)(Weights)(Times)(Bias)(Plus)
    Step         Stack                           Action
    1            (Input)                         Push
    2            (Input)(Weights)                Push
    3.1          (Input)(Weights)(Times)         Push
    3.2          (InnerProduct)                  Reduce
    4            (InnerProduct)(Bias)            Push
    5.1          (InnerProduct)(Bias)(Plus)      Push
    5.2          (InnerProductWithBias)          Reduce