#!/usr/bin/env python
from argparse import ArgumentParser
from json import dump
from os.path import dirname
from re import findall
from subprocess import check_output

parser = ArgumentParser()
parser.add_argument('elf', type = str)
parser.add_argument('id', type = str)
parser.add_argument('name', type = str)
argument = parser.parse_args()
build: str = dirname(argument.elf)

with open(f'{build}/app.json', 'w', encoding = 'utf-8', newline = '\n') as f:
	dump({
	'address_space_type': 3,
	'default_cpu_id': 3,
	'filesystem_access': {
		'permissions': '0',
	},
	'is_64_bit': True,
	'is_retail': True,
	'kernel_capabilities': [
		{
			'type': 'kernel_flags',
			'value': {
				'highest_cpu_id': 3,
				'highest_thread_priority': 63,
				'lowest_cpu_id': 3,
				'lowest_thread_priority': 15,
			},
		},
		{
			'type': 'min_kernel_version',
			'value': '30',
		},
		{
			'type': 'syscalls',
			'value': dict(findall(r'(?s)<(svc\w+)>:.+?svc\s#(\w+)', check_output(['aarch64-none-elf-objdump', '-d', '--no-addresses', '--no-show-raw-insn', argument.elf], encoding = 'utf-8', text = True))),
		},
	],
	'main_thread_priority': 32,
	'main_thread_stack_size': '1000',
	'name': argument.name,
	'optimize_memory_allocation': True,
	'pool_partition': 2,
	'program_id': f'{argument.id}',
	'program_id_range_max': f'{argument.id}',
	'program_id_range_min': f'{argument.id}',
	'service_access': ['bsd:s'],
}, f, ensure_ascii = False)

with open(f'{build}/toolbox.json', 'w', encoding = 'utf-8', newline = '\n') as f:
	dump({
		'name': argument.name,
		'requires_reboot': False,
		'tid': argument.id,
	}, f, ensure_ascii = False)
