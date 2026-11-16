import importlib.util,json,struct,unittest
from pathlib import Path
path=Path(__file__).with_name('audit_uruks.py')
spec=importlib.util.spec_from_file_location('audit_uruks',path);a=importlib.util.module_from_spec(spec);spec.loader.exec_module(a)
CAT=json.loads(Path(__file__).with_name('audit_catalog.json').read_text())
class Memory(a.Reader):
 def __init__(self):self.mem={};self.next=0x100000
 def read(self,addr,size):
  try:return bytes(self.mem[addr+i] for i in range(size))
  except KeyError:return None
 def put(self,addr,data):
  for i,b in enumerate(data):self.mem[addr+i]=b
 def alloc(self,size):
  addr=self.next;self.next+=(size+255)&~255;self.put(addr,bytes(size));return addr
 def value(self,addr,value,fmt='Q'):self.put(addr,struct.pack('<'+fmt,value))
 def rec(self,name):
  addr=self.alloc(0x30);text=self.alloc(len(name)+1);self.put(text,name.encode()+b'\0');self.value(addr+1,2,'B');self.value(addr+0x20,text);return addr
 def vec(self,member,offset,names):
  pointers=[self.rec(n) for n in names];start=self.alloc(max(8,len(names)*8));self.value(member+offset,start);self.value(member+offset+8,start+len(names)*8)
  for i,p in enumerate(pointers):self.value(start+i*8,p)
 def member(self):
  m=self.alloc(0x600)
  for off,name in [(0x178,'Faction_Volcano'),(0x188,'AI_Orc_Infantry'),(0x278,'Orc_Infantry_FacA'),(0x318,'Role_Judge'),(0x328,'TribeDef_Machine')]:self.value(m+off,self.rec(name))
  self.value(m+0x4b8,84,'I');self.vec(m,0x198,[]);self.vec(m,0x1b0,[])
  visual=self.alloc(0x40);self.value(m+0x2d8,visual)
  for off,name in [(0x20,'Cha_Orc'),(0x28,'Smp_Orc_Body'),(0x30,'Smp_Orc_Head')]:self.value(visual+off,self.rec(name))
  return m
 def selected(self):
  base=0x140000000;self.put(base+0x18d93c,a.RESOLVER_SIGNATURE)
  g=self.alloc(0x6d00);manager=self.alloc(0xc0);selection=self.alloc(0x40)
  self.value(base+0x26ffaa8,g);self.value(g+0x6c68,manager);self.value(g+0x6c80,selection)
  key=self.rec('Faction_Volcano');owner=self.alloc(0x60);self.value(key+0x18,owner);self.value(manager+0xa8,owner)
  slots=self.alloc(0x520);self.value(manager+0x98,slots);self.value(manager+0xa0,2,'I')
  self.value(slots,key);pool=self.alloc(0x30);self.value(slots+8+0x20,pool)
  members=self.alloc(16);self.value(pool+0x18,members);self.value(pool+0x20,members+16)
  m=self.member();self.value(m+0x178,key);self.value(members+8,m)
  self.value(selection,key);self.value(selection+8,1,'I');self.value(selection+0x20,6,'B')
  return base,manager,selection,key,m
 def hierarchy(self):
  base,manager,selection,key,m=self.selected()
  self.put(base+0x188ce30,bytes.fromhex('48 8b 05 d1 2c e7 00 48 83 78 10 00 74 1e 48'))
  controller=self.alloc(0x20);self.value(base+0x26ffb08,controller);self.value(controller+0x10,1)
  settings=self.alloc(0x440);self.value(base+0x266b6f0,settings);self.value(settings+0x410,0x1234,'I')
  attrs=self.alloc(16);values=self.alloc(16);self.value(key,1,'B');self.value(key+8,attrs);self.value(key+0x10,values)
  self.value(attrs+4,0x1234,'I');self.value(attrs+0xa,10,'B');self.value(attrs+0xc,2,'H')
  rowrec=self.rec('Hierarchy_Row');self.value(values+8,rowrec);self.value(m+0x268,rowrec)
  self.value(selection+8,0x201,'I');self.value(selection+0x20,0,'B')
  bucket=self.at(manager,0x98)+8;rows=self.alloc(0x28);seats=self.alloc(12)
  self.value(bucket+0xf8,rows);self.value(bucket+0x100,rows+0x28)
  self.value(rows,rowrec);self.value(rows+8,seats);self.value(rows+0x10,seats+12);self.value(seats+8,1,'I')
  return base,manager,selection,key,m,settings,rows,seats
class Tests(unittest.TestCase):
 def setUp(self):self.r=Memory();self.m=self.r.member()
 def snap(self):return a.snapshot(self.r,self.m,CAT,'fixture')
 def codes(self):return [f['code'] for f in self.snap()['findings']]
 def test_normal(self):self.assertEqual(self.snap()['assessment'],'no-listed-signal-observed')
 def test_zero_based_level(self):
  self.r.value(self.m+0x4b8,79,'I');s=self.snap();self.assertEqual(s['level_raw'],79);self.assertEqual(s['level_display'],80)
  self.r.value(self.m+0x4b8,0,'I');s=self.snap();self.assertTrue(s['identity_plausible']);self.assertEqual(s['level_display'],1)
 def test_fearless_not_cheating(self):
  self.r.vec(self.m,0x198,['Quirk_Fearless']);s=self.snap();self.assertEqual(s['fearless']['state'],'present');self.assertEqual(s['findings'],[])
 def test_awe_exact(self):
  self.r.vec(self.m,0x198,['Terror_Player']);self.assertEqual(self.snap()['overwhelming_awe']['state'],'present')
 def test_bad_pointer_unknown(self):
  self.assertFalse(a.snapshot(self.r,123,CAT,'fixture')['identity_plausible'])
 def test_unreadable_tribe_not_null(self):
  for i in range(8):del self.r.mem[self.m+0x328+i]
  self.assertNotIn('missing-or-explicitly-tribeless',self.codes())
 def test_null_tribe(self):self.r.value(self.m+0x328,0);self.assertIn('missing-or-explicitly-tribeless',self.codes())
 def test_explicit_tribeless(self):self.r.value(self.m+0x328,self.r.rec('TribeDef_Tribeless'));self.assertIn('missing-or-explicitly-tribeless',self.codes())
 def test_merc(self):self.r.vec(self.m,0x198,['Easterling_Fearless']);self.assertIn('mercenary-context-trait',self.codes())
 def test_captured_east_epic_traits(self):
  names=['East_ET_DamageBoostVsRage','East_ET_DefenseBoostVsRage','East_ET_HealthOnKill',
         'East_ET_DamageBoostEnemyHealthLow','East_ET_DamageBoostSelfHealthHigh',
         'East_ET_DefenseBoostSelfHealthHigh','East_ET_DefenseBoostSelfHealthLow','East_ET_DamageBoostSelfHealthLow']
  self.r.vec(self.m,0x198,names);s=self.snap()
  self.assertEqual(s['assessment'],'review-required')
  self.assertEqual({f['field'] for f in s['findings'] if f['code']=='mercenary-context-trait'},set(names))
 def test_deadly_alone_not_cheating(self):
  self.r.vec(self.m,0x198,['Quirk_Deadly','Quirk_AttacksFaster','Quirk_LifeSteal'])
  self.assertEqual(self.codes(),[])
 def test_east_prefix_alone_not_flagged(self):
  self.r.vec(self.m,0x198,['East_ET_UnverifiedName']);self.assertEqual(self.codes(),[])
 def test_ordinary_immunities(self):self.r.vec(self.m,0x198,['Immune_Combat','Immune_Ranged','Immune_Fire']);self.assertEqual(self.codes(),[])
 def test_special_ai(self):self.r.value(self.m+0x188,self.r.rec('AI_Balrog'));self.assertIn('special-character-record',self.codes())
 def test_class_castamir(self):self.r.value(self.m+0x278,self.r.rec('Gondorian_Castamir'));self.assertIn('special-character-record',self.codes())
 def test_cosmetic_substring_not_flagged(self):
  v=self.r.at(self.m,0x2d8);self.r.value(v+0x28,self.r.rec('Smp_Orc_Slim_NazgulArm'));self.assertNotIn('special-character-record',self.codes())
 def test_unreadable_trait_unknown(self):
  self.r.value(self.m+0x198,123);self.r.value(self.m+0x1a0,131);self.assertEqual(self.snap()['fearless']['state'],'unknown')
 def test_vector_bad_stride(self):
  self.r.value(self.m+0x198,123);self.r.value(self.m+0x1a0,132);self.assertEqual(a.vector(self.r,self.m,0x198)['status'],'invalid-or-over-bound')
 def test_vector_limit(self):
  self.r.value(self.m+0x198,100);self.r.value(self.m+0x1a0,100+513*8);self.assertEqual(a.vector(self.r,self.m,0x198)['status'],'invalid-or-over-bound')
 def test_name_flag(self):
  p=self.r.rec('name');self.r.value(p+1,0,'B');self.assertEqual(a.record(self.r,p)['status'],'name-flag-absent')
 def test_selected(self):
  base,manager,selection,key,m=self.r.selected();s=a.selected_member(self.r,base);self.assertEqual(s['member'],m)
 def test_owner_mismatch(self):
  base,manager,selection,key,m=self.r.selected();self.r.value(key+0x18,123);self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_selected_oob(self):
  base,manager,selection,key,m=self.r.selected();self.r.value(selection+8,5,'I');self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_selected_signature(self):
  base,manager,selection,key,m=self.r.selected();self.r.put(base+0x18d93c,b'\0');self.assertEqual(a.selected_member(self.r,base)['status'],'resolver-signature-mismatch')
 def test_faction_mismatch(self):
  base,manager,selection,key,m=self.r.selected();self.r.value(m+0x178,0);self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_max_buckets(self):
  base,manager,selection,key,m=self.r.selected();self.r.value(manager+0xa0,4097,'I');self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_ambiguous_selection(self):
  base,manager,selection,key,m=self.r.selected()
  slots=self.r.at(manager,0x98);pool=self.r.at(slots+8,0x20);start=self.r.at(pool,0x18)
  other=self.r.member();self.r.value(other+0x178,key);self.r.value(start,other)
  self.r.value(selection+0x28,key);self.r.value(selection+0x30,0,'I')
  self.assertEqual(a.selected_member(self.r,base)['status'],'no-unique-resolved-member')
 def test_selection_change(self):
  base,manager,selection,key,m=self.r.selected();original=self.r.read;first=[True]
  def changed(address,size):
   value=original(address,size)
   if address==selection and size==0x39 and first[0]:
    first[0]=False;self.r.value(selection+0x20,7,'B')
   return value
  self.r.read=changed
  self.assertEqual(a.selected_member(self.r,base)['status'],'selection-changed-during-read')
 def test_member_identity_change(self):
  original=self.r.read;first=[True]
  def changed(address,size):
   value=original(address,size)
   if address==self.m+0x168 and size==0x18 and first[0]:
    first[0]=False;self.r.value(self.m+0x168,9)
   return value
  self.r.read=changed
  self.assertEqual(self.snap()['assessment'],'unknown-member-identity')
 def test_snapshot_is_read_only(self):
  before=self.r.mem.copy();self.snap();self.assertEqual(before,self.r.mem)
 def test_hierarchy_seat_not_roster_index(self):
  base,manager,selection,key,m,*_=self.r.hierarchy()
  before=self.r.mem.copy();s=a.selected_member(self.r,base)
  self.assertEqual(s['member'],m);self.assertEqual(s['lookup'],'hierarchy-faction-row-seat');self.assertEqual(before,self.r.mem)
 def test_hierarchy_wrong_member_row(self):
  base,manager,selection,key,m,*_=self.r.hierarchy();self.r.value(m+0x268,0)
  self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_hierarchy_seat_out_of_bounds(self):
  base,manager,selection,key,m,*_=self.r.hierarchy();self.r.value(selection+9,3,'B')
  self.assertIsNone(a.selected_member(self.r,base)['member'])
 def test_hierarchy_alternate_schema_rejected(self):
  base,manager,selection,key,m,settings,*_=self.r.hierarchy();self.r.value(settings+0x420,2,'i')
  s=a.selected_member(self.r,base);self.assertIsNone(s['member']);self.assertEqual(s['diagnostic']['hierarchy']['status'],'alternate-row-schema-not-supported')
 def test_unresolved_has_diagnostics(self):
  base,manager,selection,key,m=self.r.selected();self.r.value(selection+8,999,'I')
  s=a.selected_member(self.r,base);self.assertEqual(len(s['diagnostic']['raw_selection'].split()),0x39)
  self.assertEqual(s['diagnostic']['references'][0]['index'],999)
 def test_empty_global_selection_is_not_member_failure(self):
  base,manager,selection,key,m=self.r.selected();self.r.put(selection,bytes(0x39))
  s=a.selected_member(self.r,base);self.assertEqual(s['status'],'global-selection-empty');self.assertIsNone(s['member'])
if __name__=='__main__':unittest.main()
