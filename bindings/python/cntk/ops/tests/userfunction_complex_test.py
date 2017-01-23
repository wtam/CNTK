# Copyright (c) Microsoft. All rights reserved.
# Licensed under the MIT license. See LICENSE.md file in the project root
# for full license information.
# ==============================================================================

"""
Unit tests for function extension
"""

from __future__ import division, print_function
import numpy as np

from cntk import *
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
    my_sig_instance.append(r)
    return r

def fully_connected_classifier_net(input, num_output_classes, hidden_layer_dim,
                                   num_hidden_layers, nonlinearity):
    h = dense_layer(input, hidden_layer_dim, nonlinearity)
    for i in range(1, num_hidden_layers):
        h = dense_layer(h, hidden_layer_dim, nonlinearity)
    r = linear_layer(h, num_output_classes)
    return r

def print_training_progress(trainer, mb, frequency):
    training_loss = "NA"
    eval_error = "NA"

    if mb%frequency == 0:
        training_loss = get_train_loss(trainer)
        eval_error = get_train_eval_criterion(trainer)

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
                                                         training_progress_output_freq)
        if not (loss == "NA" or error =="NA"):
            losses.append(loss)
            errors.append(error)

    return losses, errors


class MySigmoid(UserFunction):
    def __init__(self, arg, name='MySigmoid'):
        outputs = [output_variable(arg.shape, arg.dtype, arg.dynamic_axes)]
        super(MySigmoid, self).__init__([arg], outputs,
                name=name)

    def forward(self, arguments, outputs, device=None, outputs_to_retain=None):
        for k in outputs:
            sigmoid_x = 1/(1+np.exp(-arguments[0]))
            outputs[k] = sigmoid_x
            break

        return sigmoid_x, outputs

    def backward(self, state, root_gradients, variables):
        sigmoid_x = state

        for rk, rv in root_gradients.items():
            break
        for var_key in variables:
            break

        variables[var_key] = rv * sigmoid_x * (1 - sigmoid_x)

def test_ext_user_sigmoid():
    np.random.seed(0)
    act_losses, act_errors = train(MySigmoid)
    np.random.seed(0)
    exp_losses, exp_errors = train(sigmoid)
    assert np.allclose(exp_losses, act_losses)
    assert np.allclose(exp_errors, act_errors)

