from __future__ import print_function
import numpy as np
import sys
import os
from cntk import *
from cntk.device import cpu, set_default_device
from cntk.learner import *
from cntk.ops import *
from cntk.ops.functions import UserFunction

from cntk.utils import get_train_eval_criterion, get_train_loss

np.random.seed(0)

input_dim = 2
num_output_classes = 2


def generate_random_data_sample(sample_size, feature_dim, num_classes):
    # Create synthetic data using NumPy.
    Y = np.random.randint(size=(sample_size, 1), low=0, high=num_classes)

    # Make sure that the data is separable
    X = (np.random.randn(sample_size, feature_dim)+3) * (Y+1)
    X = X.astype(np.float32)
    class_ind = [Y==class_number for class_number in range(num_classes)]
    Y = np.asarray(np.hstack(class_ind), dtype=np.float32)
    return X, Y


def linear_layer(input_var, output_dim):
    input_dim = input_var.shape[0]
    times_param = parameter(shape=(input_dim, output_dim))
    bias_param = parameter(shape=(output_dim))

    t = times(input_var, times_param)
    return bias_param + t

my_sig_instance = []
def dense_layer(input, output_dim, nonlinearity):
    global my_sig_instance
    r = linear_layer(input, output_dim)
    r = nonlinearity(r)
    print("appending %s"%r)
    my_sig_instance.append(r)
    return r

def fully_connected_classifier_net(input, num_output_classes, hidden_layer_dim,
                                   num_hidden_layers, nonlinearity):
    h = dense_layer(input, hidden_layer_dim, nonlinearity)
    for i in range(1, num_hidden_layers):
        h = dense_layer(h, hidden_layer_dim, nonlinearity)
    r = linear_layer(h, num_output_classes)
    return r

def print_training_progress(trainer, mb, frequency, verbose=1):
    training_loss = "NA"
    eval_error = "NA"

    if mb%frequency == 0:
        training_loss = get_train_loss(trainer)
        eval_error = get_train_eval_criterion(trainer)
        if verbose:
            print ("Minibatch: {}, Train Loss: {}, Train Error: {}".format(mb, training_loss, eval_error))

    return mb, training_loss, eval_error

def train(nonlinearity):
    learning_rate = 0.5
    lr_schedule = learning_rate_schedule(learning_rate, UnitType.minibatch)

    mysamplesize = 64
    features, labels = generate_random_data_sample(mysamplesize, input_dim, num_output_classes)

    num_hidden_layers = 2
    hidden_layers_dim = 50

    input = input_variable((input_dim), np.float32)
    label = input_variable((num_output_classes), np.float32)

    z = fully_connected_classifier_net(input, num_output_classes, hidden_layers_dim,
                                       num_hidden_layers, nonlinearity)

    loss = cross_entropy_with_softmax(z, label)
    eval_error = classification_error(z, label)

    learner = sgd(z.parameters, lr_schedule)
    trainer = Trainer(z, loss, eval_error, [learner])


    minibatch_size = 25
    num_samples = 20000
    num_minibatches_to_train = num_samples / minibatch_size

    training_progress_output_freq = 20

    losses = []
    errors = []

    for i in range(0, int(num_minibatches_to_train)):
        features, labels = generate_random_data_sample(minibatch_size, input_dim, num_output_classes)

        # Specify the input variables mapping in the model to actual minibatch data for training
        trainer.train_minibatch({input : features, label : labels})
        batchsize, loss, error = print_training_progress(trainer, i,
                                                         training_progress_output_freq, verbose=0)
        if not (loss == "NA" or error =="NA"):
            print(batchsize, loss, error)
            losses.append(loss)
            errors.append(error)

    return losses, errors

class Plus3Func(UserFunction):
    def __init__(self, arg, name='f1'):
        outputs = [output_variable(arg.shape, arg.dtype, arg.dynamic_axes)]
        super(Plus3Func, self).__init__([arg], outputs,
                name=name)

        self.forward_calls = 0
        self.backward_calls = 0

    def forward(self, arguments, outputs, device=None, outputs_to_retain=None):
        assert len(self.inputs)==1
        assert len(outputs)==1

        for k in outputs:
            outputs[k] = arguments[0] + 3
            break

        self.forward_calls += 1

        return None, outputs

    def backward(self, state, root_gradients, variables):
        assert len(root_gradients) == 1
        assert len(variables) == 1
        import ipdb;ipdb.set_trace()

        for rk, rv in root_gradients.items():
            break
        for var_key in variables:
            break

        self.backward_calls += 1

        variables[var_key] = rv

class MySigmoid(UserFunction):
    def __init__(self, arg, name='MySigmoid'):
        outputs = [output_variable(arg.shape, arg.dtype, arg.dynamic_axes)]
        super(MySigmoid, self).__init__([arg], outputs,
                name=name)

    def _sigmoid(self, x):
        return 1/(1+np.exp(-x))

    def forward(self, arguments, outputs, device=None, outputs_to_retain=None):
        assert len(self.inputs)==1
        assert len(outputs)==1

        for k in outputs:
            sigmoid_x = self._sigmoid(arguments[0])
            outputs[k] = sigmoid_x
            break

        print(sigmoid_x)
        return sigmoid_x, outputs

    def backward(self, state, root_gradients, variables):
        print("state=")
        print(state)
        assert len(root_gradients) == 1
        assert len(variables) == 1

        sigmoid_x = 1

        for rk, rv in root_gradients.items():
            break
        for var_key in variables:
            break

        variables[var_key] = rv * sigmoid_x * (1 - sigmoid_x)

    def __del__(self):
        print("__del__")

def test_ext_user_sigmoid():
    act_losses, act_errors = train(MySigmoid)
    # exp_losses, exp_errors = train(sigmoid)
    # assert exp_losses == act_losses
    # assert exp_errors == act_errors

def test_ext_train_sigmoid():
    dim = 4

    label_input = input_variable(1, needs_gradient=True, name='l_var')
    p = parameter(shape=(dim,), init=10)
    data_input = input_variable(dim, needs_gradient=True, name='i_var')
    intermediate = p*data_input
    m = MySigmoid(intermediate)
    # m = sigmoid(intermediate)
    # m = Plus3Func(intermediate)
    z = m+0

    lr_per_sample = learning_rate_schedule(0.007, UnitType.sample)
    loss = squared_error(z, label_input)
    trainer = Trainer(z, loss, loss+0, \
            [sgd(z.parameters, lr_per_sample)])

    i = 0
    import threading
    while i<1000:
        print(i)
        i+=1
        input_data = np.random.rand(dim)
        label_data = np.asarray([np.sum(input_data)>1], dtype=int)
        trainer.train_minibatch({data_input:[input_data], label_input:[label_data]})

        batchsize, loss, error = print_training_progress(trainer, i, 10, verbose=0)
        if not (loss == "NA" or error =="NA"):
            print(batchsize, loss, error)

if __name__ == '__main__':
    test_ext_user_sigmoid()
