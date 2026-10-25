"""Formal fleet bridge checks against the actual C++ core."""
import json
import subprocess
import unittest
from ui.server import PROJECT, bridge_binary

class BridgeTests(unittest.TestCase):
    def setUp(self):
        self.process = subprocess.Popen([str(bridge_binary()),str(PROJECT/'config/network.json'),str(PROJECT/'config/orders.json')],
            stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=subprocess.PIPE,text=True,encoding='utf-8')
    def tearDown(self):
        self.process.stdin.close(); self.process.wait(timeout=5)
        self.process.stdout.close(); self.process.stderr.close()
    def call(self, **body):
        self.process.stdin.write(json.dumps(body)+'\n'); self.process.stdin.flush()
        line=self.process.stdout.readline()
        if not line: raise RuntimeError(self.process.stderr.read())
        return json.loads(line)
    def order(self, **kwargs):
        body=dict(id='TEST-01',warehouse='W01',destination='D05',quantity=5,priority='urgent',earliest=480,latest=720)
        if 'pickup_node_id' in kwargs: body.pop('warehouse')
        body.update(kwargs)
        return self.call(cmd='insert',order=body)
    def vehicle(self, state, vehicle_id):
        return next(v for v in state['vehicles'] if v['id']==vehicle_id)
    def finish(self):
        state=self.call(cmd='state')
        for _ in range(150):
            if state['finished']: return state
            state=self.call(cmd='advance',minutes=5); self.assertTrue(state['ok'])
            for v in state['vehicles']:
                self.assertGreaterEqual(v['load'],0); self.assertLessEqual(v['load'],v['capacity'])
        self.fail('fleet did not finish')
    def test_identity_and_shared_clock(self):
        state=self.call(cmd='state')
        self.assertEqual(state['app'],'logistics-route-planner'); self.assertEqual(state['ui_version'],2)
        self.assertFalse(state['demo']); self.assertEqual([v['id'] for v in state['vehicles']],['V01','V02'])
        self.assertEqual([v['load'] for v in state['vehicles']],[40,20])
        current=self.call(cmd='advance',minutes=1); self.assertEqual(current['time'],481)
        for old,new in zip(state['vehicles'],current['vehicles']):
            self.assertAlmostEqual(new['segment']['remaining'],old['segment']['remaining']-1)
    def test_accept_only_mutates_winner_then_completes(self):
        initial=self.call(cmd='state'); current=self.order(); decision=current['insertion']
        self.assertTrue(decision['accepted']); winner=decision['vehicle_id']; loser='V02' if winner=='V01' else 'V01'
        self.assertEqual(self.vehicle(current,loser),self.vehicle(initial,loser))
        self.assertEqual([v['load'] for v in current['vehicles']],[v['load'] for v in initial['vehicles']])
        self.assertIsNone(next(o for o in current['orders'] if o['id']=='TEST-01')['pickup'])
        done=self.finish(); order=next(o for o in done['orders'] if o['id']=='TEST-01' and o['accepted'])
        self.assertIsNotNone(order['pickup']); self.assertIsNotNone(order['completed']); self.assertEqual(order['vehicle_id'],winner)
        self.assertEqual(sum(v['delivered'] for v in done['vehicles']),4)
        for v in done['vehicles']:
            self.assertEqual(v['node_id'],'W01'); self.assertEqual(v['load'],0); self.assertEqual(v['tasks'],[]); self.assertIsNone(v['current_task'])
    def test_station_pickup_waiting_list(self):
        current=self.order(id='HUB-UI',pickup_node_id='T02',destination='D18',latest=900)
        self.assertTrue(current['insertion']['accepted'])
        self.assertEqual(next(p for p in current['pickup_points'] if p['node_id']=='T02')['waiting_orders'],['HUB-UI'])
        self.assertIn(dict(kind='pickup',order_id='HUB-UI',node_id='T02'),self.vehicle(current,current['insertion']['vehicle_id'])['tasks'])
        done=self.finish(); self.assertEqual(next(p for p in done['pickup_points'] if p['node_id']=='T02')['waiting_orders'],[])
        self.assertFalse(self.order(id='BAD',pickup_node_id='D12')['insertion']['accepted'])
    def test_rejections_preserve_both_vehicles(self):
        initial=self.call(cmd='state')
        for name,args in [('overload',dict(quantity=2000)),('window',dict(latest=480.01,destination='D25'))]:
            with self.subTest(name=name):
                result=self.order(id=name,**args); self.assertFalse(result['insertion']['accepted'])
                self.assertEqual(result['vehicles'],initial['vehicles']); self.assertEqual(result['pickup_points'],initial['pickup_points'])
                self.assertIn('V01',result['insertion']['reason']); self.assertIn('V02',result['insertion']['reason'])
    def test_shared_traffic_locks_current_segments(self):
        before=self.call(cmd='state'); road=before['vehicles'][0]['edge_id']; duration=next(e for e in before['edges'] if e['id']==road)['time']
        after=self.call(cmd='traffic',road_id=road,minutes=duration*2)
        self.assertEqual(next(e for e in after['edges'] if e['id']==road)['time'],duration*2)
        for old in before['vehicles']:
            if old['edge_id']==road: self.assertEqual(self.vehicle(after,old['id'])['segment'],old['segment'])
        advanced=self.call(cmd='advance',minutes=1)
        self.assertAlmostEqual(advanced['vehicles'][0]['segment']['remaining'],before['vehicles'][0]['segment']['remaining']-1)
    def test_invalid_duplicate_and_generated_ids(self):
        self.assertFalse(self.order(quantity=-1)['ok']); self.assertTrue(self.call(cmd='state')['ok'])
        self.assertTrue(self.order(id='MANUAL1')['insertion']['accepted']); self.assertFalse(self.order(id='MANUAL1')['insertion']['accepted'])
        generated=self.order(id=''); self.assertTrue(generated['insertion']['accepted']); self.assertEqual(generated['insertion']['id'],'MANUAL2')
        self.assertFalse(self.call(cmd='advance',minutes=1000)['ok']); self.assertFalse(self.call(cmd='traffic',road_id='missing')['ok'])
        self.assertFalse(self.order(pickup_node_id='T02',warehouse='W01')['ok'])
    def test_pending_task_sequences_survive_repair(self):
        self.assertTrue(self.order(id='SAME-A')['insertion']['accepted']); before=self.order(id='SAME-B')
        self.assertTrue(before['insertion']['accepted'])
        owners={o['id']:o['vehicle_id'] for o in before['orders'] if o['accepted']}
        for order_id in ['SAME-A','SAME-B']:
            actions=[(t['kind'],t['order_id']) for t in self.vehicle(before,owners[order_id])['tasks']]
            self.assertLess(actions.index(('pickup',order_id)),actions.index(('delivery',order_id)))
        current=self.call(cmd='traffic',road_id=before['vehicles'][0]['edge_id'])
        for a,b in zip(before['vehicles'],current['vehicles']): self.assertEqual(a['tasks'],b['tasks'])
        for _ in range(150):
            current=self.call(cmd='advance',minutes=1)
            if next(o for o in current['orders'] if o['id']=='SAME-A')['pickup'] is not None: break
        self.assertFalse(any(t['kind']=='pickup' and t['order_id']=='SAME-A' for t in self.vehicle(current,owners['SAME-A'])['tasks']))
    def test_locked_unloading_task_is_retained(self):
        current=self.call(cmd='state'); active=None
        for _ in range(600):
            active=next((v for v in current['vehicles'] if v['status']=='unloading'),None)
            if active: break
            current=self.call(cmd='advance',minutes=.5)
        self.assertIsNotNone(active)
        inserted=self.order(id='WHILE-UNLOADING',latest=900,quantity=1); self.assertTrue(inserted['insertion']['accepted'])
        self.assertEqual(self.vehicle(inserted,active['id'])['current_task'],active['current_task'])
        for _ in range(8): current=self.call(cmd='advance',minutes=.5)
        self.assertFalse(any(t['order_id']==active['current_task']['order_id'] for t in self.vehicle(current,active['id'])['tasks']))

if __name__=='__main__': unittest.main(verbosity=2)
