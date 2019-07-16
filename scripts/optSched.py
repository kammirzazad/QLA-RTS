#!/usr/bin/python

#
#  This script calculates the optimal link delay budgets for T-RADF graph based network on conditions
#    and saves the graphs with baseline (uniform distribution) and optimized schedules in 
#      /graphs/scheduled/baseline/ & /graphs/scheduled/optimized/, respectively.
#

import	sys
import	json
from	pprint		import pprint
from	scheduler	import Scheduler

schedDir = '../graphs/scheduled/'
path2net = '../networks/gamma100.ip.json'
	
def	getPercent(ref,new):
	return (100*(new-ref))/abs(ref)

def	instantiate(path2graph):

	with open(path2graph) as fh:
		graphDesc = json.load(fh)

	with open(path2net) as fh:
		networkDesc = json.load(fh)	

	return Scheduler(graphDesc, networkDesc)


def	saveGraph(graphJSON,gn):
	fn = gn + '.tradf.json'
	with open(fn,'w') as fh:
		json.dump(graphJSON, fh, indent=4, sort_keys=True)

def	scheduleAll(path2graph):
	exe = {}
	dSNR = {}
	s = instantiate(path2graph)

	for rho in [0.25,0.5,0.75,1.0]:
		res = s.optimize_by_rho(rho)
		exe[rho] = round(res['execution_time'],3)
		dSNR[rho] = getPercent(res['estSNR0'],res['estSNR1'])

	return {'deltaSNR(%)':dSNR, 'execution_time(sec)':exe}

def	schedule(path2graph,rho,dump_baseline,dump_optimized):
	s = instantiate(path2graph)	
	res = s.optimize_by_rho(rho)

	bn = path2graph
	if '/' in path2graph:
		bn = bn.split('/')[-1]
	bn = bn.split('.tradf.json')[0] + '_rho_' + str(rho)

	if dump_baseline:
		saveGraph(res['baseline_graph'],schedDir+'/baseline/'+bn)

	if dump_optimized:
		saveGraph(res['optimized_graph'],schedDir+'/optimized/'+bn)

	return	{\
			'estSNR0':res['estSNR0'],\
			'estSNR1':res['estSNR1'],\
			'deltaSNR(%)':getPercent(res['estSNR0'],res['estSNR1']),\
			'execution_time(sec)':round(res['execution_time'],3),\
			'graph_stat':res['graph_stat']
		}


if __name__ == "__main__":
	
	if len(sys.argv) != 3:
		print 'usage:', sys.argv[0], '[TRADF graph] [rho]'
		exit(1)
	
	pprint(schedule(sys.argv[1],float(sys.argv[2]),True,True))

