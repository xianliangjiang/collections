
from mininet.link import TCLink, TCIntf, Link
from mininet.topo import Topo
import termcolor as T

# Just for some fancy color printing
def cprint(s, color, cr=True):
    """Print in color
       s: string to print
       color: color to use"""
    if cr:
        print T.colored(s, color)
    else:
        print T.colored(s, color),

        
# Topology to be instantiated in Mininet
class StarTopo(Topo):
    "Star topology for DCTCP experiment"

    def __init__(self, n=3, cpu=None, bw_host=None, bw_net=None,
                 delay=None, maxq=None, enable_dctcp=None, enable_red=None,
                 show_mininet_commands=False, red_params=None):
        # Add default members to class.
        super(StarTopo, self ).__init__()
        self.n = n
        self.cpu = cpu
        self.bw_host = bw_host
        self.bw_net = bw_net
        self.delay = delay
        self.maxq = maxq
        self.enable_dctcp = enable_dctcp
        self.enable_red = enable_red
        self.red_params = red_params
        self.show_mininet_commands = show_mininet_commands;
        
        cprint("Enable DCTCP: %d" % self.enable_dctcp, 'green')
        cprint("Enable RED: %d" % self.enable_red, 'green')
        
        self.create_topology()

    # Create the experiment topology 
    # Set appropriate values for bandwidth, delay, 
    # and queue size 
    def create_topology(self):
        # Host and link configuration
        hconfig = {'cpu': self.cpu}

        if self.enable_dctcp: 
	    cprint("Enabling ECN for senders/receiver",'green')
        
	# Set configurations for the topology and then add hosts etc.
        lconfig_sender = {'bw': self.bw_host, 'delay': self.delay,
                          'max_queue_size': self.maxq,
                          'show_commands': self.show_mininet_commands}
        lconfig_receiver = {'bw': self.bw_net, 'delay': 0,
                            'max_queue_size': self.maxq,
                            'show_commands': self.show_mininet_commands}                            
        lconfig_switch = {'bw': self.bw_net, 'delay': 0,
                            'max_queue_size': self.maxq,
                            'enable_ecn': 1 if self.enable_dctcp else 0,
                            'enable_red': 1 if self.enable_red else 0,
                            'red_params': self.red_params if ( (self.enable_red or self.enable_dctcp) 
						and self.red_params != None) else None,
                            'show_commands': self.show_mininet_commands}                            
        
        n = self.n
        # Create the receiver
        receiver = self.addHost('h0')
        # Create the switch
        switch = self.addSwitch('s0')
        # Create the sender hosts
        hosts = []
        for i in range(n-1):
            hosts.append(self.addHost('h%d' % (i+1), **hconfig))
        # Create links between receiver and switch
	self.addLink(receiver, switch, cls=Link,
                      cls1=TCIntf, cls2=TCIntf,
                      params1=lconfig_receiver, params2=lconfig_switch)
        # Create links between senders and switch
        for i in range(n-1):
	    self.addLink(hosts[i], switch, **lconfig_sender)
