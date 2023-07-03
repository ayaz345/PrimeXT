import sys
import struct
import os

rootdir = sys.argv[1]
filename = sys.argv[2]
pakfile = open(filename,'wb')
offset = 12
file_entries = []

pakfile.write(struct.Struct('<4s2l').pack(b'PACK', 0, 0))

for root, subfolders, files in os.walk(rootdir):
	for file in files:
		impfilename = os.path.join(root, file)
		entry = {'filename': os.path.relpath(impfilename, rootdir).replace('\\', '/')}
		if entry['filename'].startswith('.git'):
			continue

		with open(impfilename, 'rb') as importfile:
			pakfile.write(importfile.read())
			entry['offset'] = offset
			entry['length'] = importfile.tell()
			offset += entry['length']

		file_entries.append(entry)

for entry in file_entries:
	pakfile.write(struct.Struct('<56s').pack(entry['filename'].encode('ascii')))
	pakfile.write(struct.Struct('<l').pack(entry['offset']))
	pakfile.write(struct.Struct('<l').pack(entry['length']))

pakfile.seek(0)
pakfile.write(struct.Struct('<4s2l').pack(b'PACK', offset, len(file_entries) * 64))

print(f'Done, {len(file_entries)} files packed to {filename}.')