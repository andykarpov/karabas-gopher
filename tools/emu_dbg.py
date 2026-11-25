#!/usr/bin/env python3
"""Headless unreal-ng (emu-shot's driver) + WebAPI calls for debugging.

  emu_dbg.py [--machine TSL] [--sd IMG] [--file F] STEP...
Steps: the emu-shot ones (wait:, key:, type:, shot:) and
  get:/PATH         GET /api/v1/emulator/<id>/PATH, prints the JSON
  post:/PATH:JSON   POST with a JSON body
  paths:WORD        list the OpenAPI paths containing WORD
"""
import importlib.machinery, importlib.util, json, os, sys, tempfile, urllib.request

loader = importlib.machinery.SourceFileLoader('emu_shot', os.path.expanduser('~/.local/bin/emu-shot'))
spec = importlib.util.spec_from_loader('emu_shot', loader)
es = importlib.util.module_from_spec(spec)
loader.exec_module(es)


def main():
    import argparse
    ap = argparse.ArgumentParser()
    ap.add_argument('--machine', default='TSL')
    ap.add_argument('--sd', default=os.path.expanduser('~/.local/share/emu-shot/tsconf-sd.img'))
    ap.add_argument('--file')
    ap.add_argument('steps', nargs='+')
    a = ap.parse_args()
    a.extra = []
    with tempfile.TemporaryDirectory(prefix='emu-dbg-') as home:
        emulator, s = es.start_unreal(a, home)
        try:
            s.wait_ready()
            s.start_machine(a.machine, a.file, a.sd)
            for step in a.steps:
                kind, _, value = step.partition(':')
                if kind == 'get':
                    print(step, json.dumps(s.request('GET', '/%s%s' % (s.id, value))))
                elif kind == 'post':
                    path, _, body = value.partition(':')
                    print(step, json.dumps(s.request('POST', '/%s%s' % (s.id, path), json.loads(body or '{}'))))
                elif kind == 'paths':
                    url = s.base.rsplit('/emulator', 1)[0] + '/openapi.json'
                    doc = json.load(urllib.request.urlopen(url))
                    for p in doc['paths']:
                        if value.lower() in p.lower():
                            print(p, ' '.join(doc['paths'][p].keys()))
                else:
                    s.run_step(step)
        finally:
            es.stop(emulator)


if __name__ == '__main__':
    main()
