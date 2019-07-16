# QLARS
This repository contains scripts, graphs and simulation models for "Quality/Latency-Aware Real-time Scheduling of Distributed Streaming IoT Applications" to be published at CODES+ISSS, special issue of ACM Transactions on Embedded Computing Systems (TECS), 2019.
It consists of several folders:

* scripts
* graphs
* networks
* sim-models

Below, we explain the content of each folder in detail:

## scripts
This folder contains the implementation of T-RADF, quality model and optimization algorithm in Python (tradf.py,scheduler.py). It uses [networkx](https://networkx.github.io/) library to represent the graph and operate on it. [SciPy](https://www.scipy.org/) is used to implement the optimization routine. Finally, NumPy is miscellaneously used throughout the code. This folder also includes two executable scripts (optSched.py,randSched.py) that take a graph and latency constraint as input and generate a scheduled graph. 

The graph is assumed to be in json format with actors and channels listed as value of _actors_ and _channels_ keys. Each actor has three keys: _name_, _host_ and _wcet_. 

Latency constraint is described as the constraint factor _rho_ (0<,<1). See Section 6 in the paper for formal definition of _rho_.

## graphs
This folder contains the random graphs used in experimental setup along with the graph corresponding to Distributed Neural Network application. Random graphs are divided to two sets: acyclic and cyclic. Each set is further categorized to three groups of small (with 10 nodes), medium (with 50 nodes) and large (100 nodes). This folder also contains _scheduled_ folder that is used as output directory for executable scripts.

## networks
This folder includes the network specifications used in the paper to schedule random graphs (gamma100.ip.json) and distributed neural network applications (gamma8.ip.json). As these files show, network specifications are nested Python dictionaries indexed by source and target hosts' name, stored in json format. Each entry in these dictionaries need to have 4 or 5 properties: _dist_, _loc_, _scale_, _shape_ and _u_ which are probabilistic distribution's name, delay offset to shift distribution by in milliseconds (e.g. mean of normal distribution), scale paramateter of distribution in milliseconds (e.g. standard deviation of normal distribution), unitless shape parameter of distribution if needed (e.g. for gamma distribution) and average loss rate (between 0 and 1), respectively. Distribution names follow [the SciPy convention](https://docs.scipy.org/doc/scipy-0.16.1/reference/stats.html).

## sim-models
This folder includes the simulation models for random graphs and distributed neural network application. They were developed using OMNET++ simulator and INET Framework 3.6.4-394571f. To simulate a scheduled random graph, you will need to set _**.graph_ variable in omnetpp.ini to point to it, which has a default value of _scheduled.tradf.json_. Furthermore, to be able to compile the simulation model, you will need to install [json library for C++](https://packages.debian.org/sid/libjsoncpp-dev). Simulation model for distributed neural network has no external dependecies and once compiled, could simulate baseline and optimized scheduled for _rho_values of 0.2, 0.25, 0.4, 0.5, 0.75 and 1.0. Note that you can simulate different configurations by modifying its omnetpp.ini.


If you have a question about this repository or a problem running scripts/models, you can contact Kamyar at kammirzazad@utexas.edu. 

