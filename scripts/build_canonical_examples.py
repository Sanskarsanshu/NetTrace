#!/usr/bin/env python3
"""
Generates the 12 starter and test topology files for NetTrace 3D in examples/
strictly matching TEST_VECTORS.md section 4.
"""

import json
from pathlib import Path

def mac(node_idx: int, iface_idx: int) -> str:
    n_hi = (node_idx >> 8) & 0xFF
    n_lo = node_idx & 0xFF
    i_hi = (iface_idx >> 8) & 0xFF
    i_lo = iface_idx & 0xFF
    return f"02:00:{n_hi:02x}:{n_lo:02x}:{i_hi:02x}:{i_lo:02x}"

def make_interface(if_id: str, name: str, ipv4: str = None, mac_addr: str = None, mtu: int = None):
    ip_addr = None
    prefix = None
    if ipv4:
        ip_addr, pref_str = ipv4.split("/")
        prefix = int(pref_str)
    return {
        "id": if_id,
        "name": name,
        "adminState": "up",
        "ipv4Address": ip_addr,
        "prefixLength": prefix,
        "macAddress": mac_addr,
        "mtuBytes": None if (mtu is None or mtu == 1500) else mtu
    }

def make_link(link_id: str, a_node: str, a_if: str, b_node: str, b_if: str,
              cost: float = 1.0, bw: int = 100000000, avail_bw: int = None,
              delay: float = 1.0, loss: float = 0.0, mtu: int = 1500,
              direction: str = "bidirectional", admin_state: str = "up"):
    return {
        "id": link_id,
        "a": {"nodeId": a_node, "interfaceId": a_if},
        "b": {"nodeId": b_node, "interfaceId": b_if},
        "adminState": admin_state,
        "directionality": direction,
        "cost": float(cost),
        "bandwidthBps": bw,
        "availableBandwidthBps": avail_bw,
        "propagationDelayMs": float(delay),
        "packetLossProbability": float(loss),
        "mtuBytes": mtu
    }

def sort_topology(topo: dict) -> dict:
    # Sort nodes by id
    topo["nodes"] = sorted(topo["nodes"], key=lambda n: n["id"])
    for n in topo["nodes"]:
        n["interfaces"] = sorted(n["interfaces"], key=lambda i: i["id"])
    # Sort links by id
    topo["links"] = sorted(topo["links"], key=lambda l: l["id"])
    return topo

def write_json(path: Path, topo: dict):
    sort_topology(topo)
    content = json.dumps(topo, indent=2) + "\n"
    path.write_text(content, encoding="utf-8")

def main():
    examples_dir = Path(__file__).resolve().parent.parent / "examples"
    examples_dir.mkdir(exist_ok=True)

    # 4.1 same-subnet.json
    same_subnet = {
        "schemaVersion": "1.0",
        "id": "same-subnet",
        "name": "Scenario A - Same Subnet",
        "description": "Two hosts on the same subnet connected through an Ethernet switch. Demonstrates direct local delivery without IP routing.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "sw1",
                "name": "Switch 1",
                "type": "switch",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("p1", "p1", None, None, 1500),
                    make_interface("p2", "p2", None, None, 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": None,
                "services": [
                    {"protocol": "UDP", "port": 5000, "label": "echo"}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.20/24", mac(2, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("l1", "host-a", "eth0", "sw1", "p1", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l2", "sw1", "p2", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "same-subnet.json", same_subnet)

    # 4.2 gateway-routing.json
    gateway_routing = {
        "schemaVersion": "1.0",
        "id": "gateway-routing",
        "name": "Scenario B - Gateway Routing",
        "description": "Two hosts in different subnets connected via an intermediate router. Demonstrates off-subnet delivery via default gateway.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "192.168.1.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.0.1/24", mac(2, 2), 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.0.0.1",
                "services": [
                    {"protocol": "UDP", "port": 5000, "label": "echo"}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.0.20/24", mac(3, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("l1", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l2", "host-b", "eth0", "r1", "eth1", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "gateway-routing.json", gateway_routing)

    # 4.3 multi-router-cloud.json
    multi_router_cloud = {
        "schemaVersion": "1.0",
        "id": "multi-router-cloud",
        "name": "Scenario C - Multi-Router Cloud Path",
        "description": "Multi-hop topology traversing three routers to a simulated cloud endpoint with static routing.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "192.168.1.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "sw1",
                "name": "Switch 1",
                "type": "switch",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("p1", "p1", None, None, 1500),
                    make_interface("p2", "p2", None, None, 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.0.35.0/30", "nextHop": "10.0.12.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.23.0/30", "nextHop": "10.0.12.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.12.1/30", mac(2, 2), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "192.168.1.0/24", "nextHop": "10.0.12.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.35.0/30", "nextHop": "10.0.23.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.12.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.23.1/30", mac(3, 2), 1500)
                ]
            },
            {
                "id": "r3",
                "name": "Router 3",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "192.168.1.0/24", "nextHop": "10.0.23.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.12.0/30", "nextHop": "10.0.23.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.23.2/30", mac(4, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.35.1/30", mac(4, 2), 1500)
                ]
            },
            {
                "id": "cloud",
                "name": "Cloud Endpoint",
                "type": "cloudEndpoint",
                "adminState": "up",
                "defaultGateway": "10.0.35.1",
                "services": [
                    {"protocol": "UDP", "port": 5000, "label": "echo"}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.35.2/30", mac(5, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("lA", "host-a", "eth0", "sw1", "p1", cost=1.0, bw=1000000000, delay=0.5, loss=0.0, mtu=1500),
            make_link("lB", "sw1", "p2", "r1", "eth0", cost=1.0, bw=1000000000, delay=0.5, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=5.0, bw=100000000, delay=5.0, loss=0.0, mtu=1500),
            make_link("l23", "r2", "eth1", "r3", "eth0", cost=8.0, bw=50000000, delay=12.0, loss=0.01, mtu=1500),
            make_link("l3c", "r3", "eth1", "cloud", "eth0", cost=2.0, bw=100000000, delay=2.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "multi-router-cloud.json", multi_router_cloud)

    # 4.4 ttl-expiry.json
    ttl_expiry = {
        "schemaVersion": "1.0",
        "id": "ttl-expiry",
        "name": "TTL Expiry",
        "description": "Multi-hop path designed for demonstrating hop-by-hop TTL decrement and ICMP Time Exceeded generation.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.1.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.2.0.0/24", "nextHop": "10.12.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.23.0.0/30", "nextHop": "10.12.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.12.0.1/30", mac(2, 2), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.1.0.0/24", "nextHop": "10.12.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.2.0.0/24", "nextHop": "10.23.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.12.0.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.23.0.1/30", mac(3, 2), 1500)
                ]
            },
            {
                "id": "r3",
                "name": "Router 3",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.1.0.0/24", "nextHop": "10.23.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.12.0.0/30", "nextHop": "10.23.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.23.0.2/30", mac(4, 1), 1500),
                    make_interface("eth1", "eth1", "10.2.0.1/24", mac(4, 2), 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.2.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.2.0.20/24", mac(5, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("l1", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l23", "r2", "eth1", "r3", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l3", "r3", "eth1", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "ttl-expiry.json", ttl_expiry)

    # 4.5 no-route.json
    no_route = {
        "schemaVersion": "1.0",
        "id": "no-route",
        "name": "No Route",
        "description": "Topology containing deliberate routing gaps to demonstrate NO_ROUTE errors at host and router levels.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "host-d",
                "name": "Host D",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "192.168.1.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.30/24", mac(2, 1), 1500)
                ]
            },
            {
                "id": "sw1",
                "name": "Switch 1",
                "type": "switch",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("p1", "p1", None, None, 1500),
                    make_interface("p2", "p2", None, None, 1500),
                    make_interface("p3", "p3", None, None, 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.1/24", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.0.1/24", mac(3, 2), 1500),
                    make_interface("eth2", "eth2", "10.9.0.1/24", mac(3, 3), 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.0.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.0.20/24", mac(4, 1), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "192.168.1.0/24", "nextHop": "10.9.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.0.0/24", "nextHop": "10.9.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.9.0.2/24", mac(5, 1), 1500),
                    make_interface("eth1", "eth1", "172.16.0.1/24", mac(5, 2), 1500)
                ]
            },
            {
                "id": "host-c",
                "name": "Host C",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "172.16.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "172.16.0.10/24", mac(6, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("lA", "host-a", "eth0", "sw1", "p1", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lD", "host-d", "eth0", "sw1", "p2", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lR1", "r1", "eth0", "sw1", "p3", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l1b", "r1", "eth1", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth2", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l2c", "r2", "eth1", "host-c", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "no-route.json", no_route)

    # 4.6 Diamond topology shared by bandwidth-vs-cost.json and link-failover.json
    def make_diamond_nodes():
        return [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.1.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.2.0.0/24", "nextHop": "10.12.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.12.0.1/30", mac(2, 2), 1500),
                    make_interface("eth2", "eth2", "10.13.0.1/30", mac(2, 3), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.2.0.0/24", "nextHop": "10.24.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.1.0.0/24", "nextHop": "10.12.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.12.0.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.24.0.1/30", mac(3, 2), 1500)
                ]
            },
            {
                "id": "r3",
                "name": "Router 3",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.2.0.0/24", "nextHop": "10.34.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.1.0.0/24", "nextHop": "10.13.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.13.0.2/30", mac(4, 1), 1500),
                    make_interface("eth1", "eth1", "10.34.0.1/30", mac(4, 2), 1500)
                ]
            },
            {
                "id": "r4",
                "name": "Router 4",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.1.0.0/24", "nextHop": "10.24.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.24.0.2/30", mac(5, 1), 1500),
                    make_interface("eth1", "eth1", "10.34.0.2/30", mac(5, 2), 1500),
                    make_interface("eth2", "eth2", "10.2.0.1/24", mac(5, 3), 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.2.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.2.0.20/24", mac(6, 1), 1500)
                ]
            }
        ]

    # 4.6 bandwidth-vs-cost.json
    bw_vs_cost = {
        "schemaVersion": "1.0",
        "id": "bandwidth-vs-cost",
        "name": "Bandwidth vs Cost",
        "description": "Diamond topology demonstrating trade-offs between low-cost narrow paths vs higher-cost wider paths.",
        "nodes": make_diamond_nodes(),
        "links": [
            make_link("ha", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=1000000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=1.0, bw=10000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l24", "r2", "eth1", "r4", "eth0", cost=1.0, bw=10000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l13", "r1", "eth2", "r3", "eth0", cost=5.0, bw=1000000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l34", "r3", "eth1", "r4", "eth1", cost=5.0, bw=1000000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lb", "r4", "eth2", "host-b", "eth0", cost=1.0, bw=1000000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "bandwidth-vs-cost.json", bw_vs_cost)

    # 4.6 link-failover.json
    link_failover = {
        "schemaVersion": "1.0",
        "id": "link-failover",
        "name": "Link Failover",
        "description": "Diamond topology with dual redundant paths for link fault injection and dynamic failover routing.",
        "nodes": make_diamond_nodes(),
        "links": [
            make_link("ha", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l24", "r2", "eth1", "r4", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l13", "r1", "eth2", "r3", "eth0", cost=5.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l34", "r3", "eth1", "r4", "eth1", cost=5.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lb", "r4", "eth2", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "link-failover.json", link_failover)

    # 4.7 Plan Q shared nodes (mtu-df, fragmentation, lossy)
    def make_plan_q_nodes():
        return [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.1.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.2.0.0/24", "nextHop": "10.12.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.12.0.1/30", mac(2, 2), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.1.0.0/24", "nextHop": "10.12.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.12.0.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.2.0.1/24", mac(3, 2), 1500)
                ]
            },
            {
                "id": "host-b",
                "name": "Host B",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.2.0.1",
                "services": [
                    {"protocol": "UDP", "port": 5000, "label": "echo"}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.2.0.20/24", mac(4, 1), 1500)
                ]
            }
        ]

    # 4.7 mtu-df.json
    mtu_df = {
        "schemaVersion": "1.0",
        "id": "mtu-df",
        "name": "MTU and DF Flag",
        "description": "Path containing a bottleneck MTU link (576 bytes) for evaluating DF drop semantics.",
        "nodes": make_plan_q_nodes(),
        "links": [
            make_link("la", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lm", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=576),
            make_link("lb", "r2", "eth1", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "mtu-df.json", mtu_df)

    # 4.7 fragmentation.json
    fragmentation = {
        "schemaVersion": "1.0",
        "id": "fragmentation",
        "name": "IPv4 Fragmentation",
        "description": "Path with jumbo frame ingress link (9000 bytes) and standard links (1500 bytes) triggering router fragmentation.",
        "nodes": make_plan_q_nodes(),
        "links": [
            make_link("la", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=9000),
            make_link("lm", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lb", "r2", "eth1", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "fragmentation.json", fragmentation)

    # 4.7 lossy.json
    lossy = {
        "schemaVersion": "1.0",
        "id": "lossy",
        "name": "Lossy Links",
        "description": "Linear topology with distinct loss probabilities per link for testing cumulative path loss formulas.",
        "nodes": make_plan_q_nodes(),
        "links": [
            make_link("la", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.05, mtu=1500),
            make_link("lm", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.10, mtu=1500),
            make_link("lb", "r2", "eth1", "host-b", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "lossy.json", lossy)

    # 4.8 loop.json
    loop = {
        "schemaVersion": "1.0",
        "id": "loop",
        "name": "Routing Loop",
        "description": "Two routers with mutual static routes forming a forwarding loop for testing TTL expiration.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.1.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.9.0.0/24", "nextHop": "10.12.0.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.1.0.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.12.0.1/30", mac(2, 2), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.9.0.0/24", "nextHop": "10.12.0.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.12.0.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.2.0.1/24", mac(3, 2), 1500)
                ]
            },
            {
                "id": "host-c",
                "name": "Host C",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "10.2.0.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "10.2.0.10/24", mac(4, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("l1", "host-a", "eth0", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l2", "r2", "eth1", "host-c", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "loop.json", loop)

    # 4.9 shortcut-cost-vs-hops.json
    shortcut_cost_vs_hops = {
        "schemaVersion": "1.0",
        "id": "shortcut-cost-vs-hops",
        "name": "Shortcut Cost vs Hops",
        "description": "Multi-router topology with a low-hop high-cost direct link contrasting BFS fewest hops vs Dijkstra lowest cost.",
        "nodes": [
            {
                "id": "host-a",
                "name": "Host A",
                "type": "host",
                "adminState": "up",
                "defaultGateway": "192.168.1.1",
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.10/24", mac(1, 1), 1500)
                ]
            },
            {
                "id": "sw1",
                "name": "Switch 1",
                "type": "switch",
                "adminState": "up",
                "defaultGateway": None,
                "interfaces": [
                    make_interface("p1", "p1", None, None, 1500),
                    make_interface("p2", "p2", None, None, 1500)
                ]
            },
            {
                "id": "r1",
                "name": "Router 1",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "10.0.35.0/30", "nextHop": "10.0.13.2", "outInterface": "eth2", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.23.0/30", "nextHop": "10.0.12.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "192.168.1.1/24", mac(2, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.12.1/30", mac(2, 2), 1500),
                    make_interface("eth2", "eth2", "10.0.13.1/30", mac(2, 3), 1500)
                ]
            },
            {
                "id": "r2",
                "name": "Router 2",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "192.168.1.0/24", "nextHop": "10.0.12.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.35.0/30", "nextHop": "10.0.23.2", "outInterface": "eth1", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.12.2/30", mac(3, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.23.1/30", mac(3, 2), 1500)
                ]
            },
            {
                "id": "r3",
                "name": "Router 3",
                "type": "router",
                "adminState": "up",
                "defaultGateway": None,
                "forwardingEnabled": True,
                "staticRoutes": [
                    {"prefix": "192.168.1.0/24", "nextHop": "10.0.13.1", "outInterface": "eth2", "adminPreference": 1, "metric": 0},
                    {"prefix": "10.0.12.0/30", "nextHop": "10.0.23.1", "outInterface": "eth0", "adminPreference": 1, "metric": 0}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.23.2/30", mac(4, 1), 1500),
                    make_interface("eth1", "eth1", "10.0.35.1/30", mac(4, 2), 1500),
                    make_interface("eth2", "eth2", "10.0.13.2/30", mac(4, 3), 1500)
                ]
            },
            {
                "id": "cloud",
                "name": "Cloud Endpoint",
                "type": "cloudEndpoint",
                "adminState": "up",
                "defaultGateway": "10.0.35.1",
                "services": [
                    {"protocol": "UDP", "port": 5000, "label": "echo"}
                ],
                "interfaces": [
                    make_interface("eth0", "eth0", "10.0.35.2/30", mac(5, 1), 1500)
                ]
            }
        ],
        "links": [
            make_link("lA", "host-a", "eth0", "sw1", "p1", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("lB", "sw1", "p2", "r1", "eth0", cost=1.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l12", "r1", "eth1", "r2", "eth0", cost=5.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l23", "r2", "eth1", "r3", "eth0", cost=5.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l13", "r1", "eth2", "r3", "eth2", cost=20.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500),
            make_link("l3c", "r3", "eth1", "cloud", "eth0", cost=2.0, bw=100000000, delay=1.0, loss=0.0, mtu=1500)
        ]
    }
    write_json(examples_dir / "shortcut-cost-vs-hops.json", shortcut_cost_vs_hops)

    print(f"Generated 12 topology JSON files in {examples_dir}")

if __name__ == "__main__":
    main()
