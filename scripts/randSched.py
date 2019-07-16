#!/usr/bin/python

#
#  This script generates a random schedule for the given graph under the given latency constraint (rho)
#    and saves it as /graphs/scheduled/random.tradf.json
#

import	sys
import	json
from	random		import uniform
from	pprint		import pprint
from	scheduler	import Scheduler

path2net = '../networks/gamma100.ip.json'
defaultFN = '../graphs/scheduled/random.tradf.json'

def	instantiate(path2graph):

	with open(path2graph) as fh:
		graphDesc = json.load(fh)

	with open(path2net) as fh:
		networkDesc = json.load(fh)	

	return Scheduler(graphDesc, networkDesc)


def	saveGraph(graphJSON,gn):
	with open(gn,'w') as fh:
		json.dump(graphJSON, fh, indent=4, sort_keys=True)

def	schedule(path2graph,outFN):
	rho = round(uniform(0.1,0.9),2)
	return schedule_by_rho(path2graph,rho,outFN)

def	schedule_by_rho(path2graph,rho,outFN):
	res = instantiate(path2graph).random(rho)
	saveGraph(res['baseline_graph'],outFN)
	return	{\
			'estSNR0':res['estSNR0'],\
			'graph_stat':res['graph_stat']
		}


if __name__ == "__main__":

	if len(sys.argv) != 3:
		print 'usage:', sys.argv[0], '[TRADF graph] [rho]'
		exit(1)	

	rho = float(sys.argv[2])
	path2graph = sys.argv[1]

	pprint(schedule_by_rho(path2graph,rho,defaultFN))

