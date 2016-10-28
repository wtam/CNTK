#pragma warning(push, 1)
#include "gflags/gflags.h"
#include "glog/logging.h"
#pragma warning(pop)

#include "cntk_to_caffe_converter.h"
#include "converter_version.h"
#include <iostream>
#include <memory>

using namespace std;


// TODO: Temporary mechanism to enable memory sharing for
// node output value matrices. This will go away when the
// sharing is ready to be enabled by default
bool g_shareNodeValueMatrices = false;

void Usage()
{
    cout << "Usage:" << endl;
    cout << "cntk_to_caffe <cntk_model> <caffe_model>" << endl;
    cout << "\t<cntk_model> path to trained CNTK model" << endl;
    cout << "\t<caffe_model> binary Caffe model with weights" << endl;
    cout << "\tNote: Caffe architecture spec will be stored in <caffe_model>.model" << endl;
}

int main(int argc, char* argv[])
{
    if (argc != 3)
    {
        Usage();
        return 1;
    }
    ::google::InitGoogleLogging(argv[0]);

    CheckCntkVersion();

    static const string model_input(argv[1]);
    static const string model_output(argv[2]);

    bool is_converted = false;
    try
    {
        is_converted = ConvertCntkToCaffe(model_input, model_output);
    }
    catch (std::exception& e)
    {
        cout << e.what() << endl;
        is_converted = false;
    }

    if (is_converted)
    {
        cout << "Conversion finished!" << endl;
    }
    else
    {
        cout << "Conversion failed!" << endl;
    }

    return is_converted ? 0 : 1;
}