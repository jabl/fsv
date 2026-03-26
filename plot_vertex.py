#!/usr/bin/env python3

# SPDX-FileCopyrightText: 2021 Janne Blomqvist <blomqvist.janne@gmail.com>
# SPDX-License-Identifier: MIT

# Plots vertices of the FSV characters along with their indices.
# To use: Copypaste a vertex array from src/fsv3d.h to a text file, run this
# program with the text file as argument

import numpy as np
import matplotlib.pyplot as plt

def plot_vertex(fname):
        """Plot vertex data from file"""
        v = np.fromfile(fname, sep=',')
        v = v.reshape(-1, 2)
        fig = plt.figure()
        ax = fig.add_subplot()
        ax.plot(v[:,0], v[:,1], '+')
        for i, c in enumerate(v): 
                plt.text(c[0] + 1, c[1] + 1, str(i), color="red") 
        ax.set_aspect('equal', adjustable='box')
        plt.show()


if __name__ == '__main__':
        import sys
        plot_vertex(sys.argv[1])
