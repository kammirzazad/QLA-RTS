# QLARS
This repository contains scripts, graphs and simulation models for "Quality/Latency-Aware Real-time Scheduling of Distributed Streaming IoT Applications" to be published at CODES+ISSS, special issue of ACM Transactions on Embedded Computing Systems (TECS), 2019.
It consists of several folders:
* scripts
* graphs
* networks
* sim-models
Below, we explain content of each folder in detail:

## scripts
This folder contains the implementation of T-RADF, quality model and optimization algorithm in Python (tradf.py,scheduler.py). It uses [networkx](https://networkx.github.io/) library to represent the graph and operate on it. [SciPy](https://www.scipy.org/) is used to implement the optimization routine. Finally, NumPy is miscellaneously used throughout the code. This folder also includes two executable scripts (optSched.py,randSched.py) that take a graph and latency constraint as input and generate a scheduled graph. Input graph is assumed to be in json format. Latency constraint is described as the constraint factor _rho_ (0<,<1). See Section 6 in the paper for formal definition of _rho_.

## graphs
This folder constains the random graphs used in experimental setup along with the graph corresponding to Distributed Neural Network application. Random graphs are divided to two sets: acyclic and cyclic. Each set is further categorized to three groups of small (with 10 nodes), medium (with 50 nodes) and large (100 nodes). This folder also contains _scheduled_ folder that is used as output directory for executable scripts.

## networks


## sim-models
