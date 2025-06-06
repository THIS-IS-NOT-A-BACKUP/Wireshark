#
# Wireshark tests
# By Gerald Combs <gerald@wireshark.org>
#
# Ported from a set of Bash scripts which were copyright 2005 Ulf Lamping
#
# SPDX-License-Identifier: GPL-2.0-or-later
#
'''Dissection tests'''

import sys
import os.path
import subprocess
from subprocesstest import count_output, grep_output
import pytest


class TestDissectDtnTcpcl:
    def test_tcpclv3_xfer(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_tcpclv3_bpv6_transfer.pcapng'),
                '-Tfields',
                '-etcpcl.ack.length',
            ), encoding='utf-8', env=test_env)
        assert stdout.split() == ['1064', '1064']

    def test_tcpclv4_xfer(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_tcpclv4_bpv7_transfer.pcapng'),
                '-Tfields',
                '-etcpcl.v4.xfer_ack.ack_len',
            ), encoding='utf-8', env=test_env)
        assert stdout.split() == ['100,199,100', '199']


class TestDissectBpv7:
    '''
    The UDP test captures were generated from the BP/UDPCL example files with command:
    for FN in test/captures/dtn_udpcl*.cbordiag; do python3 tools/generate_udp_pcap.py --dport 4556 --infile $FN --outfile ${FN%.cbordiag}.pcap; done
    '''
    def test_bpv7_admin_status(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_bib_admin.pcap'),
                '-Tfields',
                '-ebpv7.status_rep.identity',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'Source: ipn:93.185, DTN Time: 1396536125, Seq: 281'

    def test_bpv7_eid_dtn(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_eid_schemes.pcap'),
                '-Tfields',
                '-ebpv7.primary.dst_uri',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'dtn://auth/svc'

    def test_bpv7_eid_ipn(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_eid_schemes.pcap'),
                '-Tfields',
                '-ebpv7.primary.src_uri',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'ipn:977000.5279.7390'

    def test_bpv7_eid_unknown(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_eid_schemes.pcap'),
                '-Tfields',
                '-ebpv7.primary.report_uri',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == ''

    def test_bpv7_eid_ipn_update(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_ipn_3comp.pcap'),
                '-Tfields',
                '-ebpv7.eid.uri',
                '-ebpv7.eid.ipn_altform',
            ), encoding='utf-8', env=test_env)
        expect = [
            'ipn:0.26622.12070,ipn:977000.5279.7390,ipn:4196183048196785.1111,ipn:93.185',
            'ipn:26622.12070,ipn:4196183048197279.7390,ipn:977000.4785.1111,ipn:0.93.185',
        ]
        assert stdout.strip() == '\t'.join(expect)

    def test_bpv7_eid_ipn_invalid(self, cmd_tshark, capture_file, test_env):
        ''' URIs are absent, not NONE or <MISSING> '''
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_ipn_invalid.pcap'),
                '-Tfields',
                '-ebpv7.primary.src_uri',
                '-ebpv7.primary.dst_uri',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == ''

    def test_bpv7_bpsec_bib(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_bib_admin.pcap'),
                '-Tfields',
                '-ebpsec.asb.ctxid',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '1'

    def test_bpv7_bpsec_bib_admin_type(self, cmd_tshark, capture_file, test_env):
        # BIB doesn't alter payload
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_bib_admin.pcap'),
                '-Tfields',
                '-ebpv7.admin_rec.type_code',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '1'

    def test_bpv7_bpsec_bcb(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_bcb_admin.pcap'),
                '-Tfields',
                '-ebpsec.asb.ctxid',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '2'

    def test_bpv7_bpsec_bcb_admin_type(self, cmd_tshark, capture_file, test_env):
        # BCB inhibits payload dissection
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_bcb_admin.pcap'),
                '-Tfields',
                '-ebpv7.admin_rec.type_code',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == ''

    def test_bpv7_bpsec_cose_mac0_result_alg(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_cose_mac0.pcap'),
                '-Tfields',
                '-ebpsec.asb.ctxid',
                '-ebpsec.asb.result.id',
                '-ecose.alg.int',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '\t'.join(['3', '17', '5'])

    def test_bpv7_bpsec_cose_encrypt_result_alg(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dtn_udpcl_bpv7_bpsec_cose_encrypt_ec.pcap'),
                '-Tfields',
                '-ebpsec.asb.ctxid',
                '-ebpsec.asb.result.id',
                '-ecose.alg.int',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '\t'.join(['3', '96', '3,-31'])


class TestDissectCose:
    '''
    These test captures were generated from the COSE example files with command:
    for FN in test/captures/cose*.cbordiag; do python3 tools/generate_cbor_pcap.py --content-type 'application/cose' --infile $FN --outfile ${FN%.cbordiag}.pcap; done
    '''
    def test_cose_sign_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_sign_tagged.pcap'),
                '-Tfields',
                '-ecose.msg.signature',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'e2aeafd40d69d19dfe6e52077c5d7ff4e408282cbefb5d06cbf414af2e19d982ac45ac98b8544c908b4507de1e90b717c3d34816fe926a2b98f53afd2fa0f30a,00a2d28a7c2bdb1587877420f65adf7d0b9a06635dd1de64bb62974c863f0b160dd2163734034e6ac003b01e8705524c5c4ca479a952f0247ee8cb0b4fb7397ba08d009e0c8bf482270cc5771aa143966e5a469a09f613488030c5b07ec6d722e3835adb5b2d8c44e95ffb13877dd2582866883535de3bb03d01753f83ab87bb4f7a0297'

    def test_cose_sign1_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_sign1_tagged.pcap'),
                '-Tfields',
                '-ecose.msg.signature',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '8eb33e4ca31d1c465ab05aac34cc6b23d58fef5c083106c4d25a91aef0b0117e2af9a291aa32e14ab834dc56ed2a223444547e01f11d3b0916e5a4c345cacb36'

    def test_cose_encrypt_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_encrypt_tagged.pcap'),
                '-Tfields',
                '-ecose.kid',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '6f75722d736563726574'

    def test_cose_encrypt0_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_encrypt0_tagged.pcap'),
                '-Tfields',
                '-ecose.iv',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '89f52f65a1c580933b5261a78c'

    def test_cose_mac_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_mac_tagged.pcap'),
                '-Tfields',
                '-ecose.msg.mac_tag',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'bf48235e809b5c42e995f2b7d5fa13620e7ed834e337f6aa43df161e49e9323e'

    def test_cose_mac0_tagged(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_mac0_tagged.pcap'),
                '-Tfields',
                '-ecose.msg.mac_tag',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '726043745027214f'

    def test_cose_keyset(self, cmd_tshark, capture_file, test_env):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('cose_keyset.pcap'),
                '-Tfields',
                '-ecose.key.k',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == '849b57219dae48de646d07dbb533566e976686457c1491be3a76dcea6c427188'


class TestDissectGprpc:
    def test_grpc_with_json(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC with JSON payload'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_person_search_json_with_image.pcapng.gz'),
                '-d', 'tcp.port==50052,http2',
                '-2',
                '-Y', 'grpc.message_length == 208 && json.value.string == "87561234"',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC/JSON')

    def test_grpc_with_protobuf(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC with Protobuf payload'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_person_search_protobuf_with_image.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-d', 'tcp.port==50051,http2',
                '-2',
                '-Y', 'protobuf.message.name == "tutorial.PersonSearchRequest"'
                      ' || (grpc.message_length == 66 && protobuf.field.value.string == "Jason"'
                      '     && protobuf.field.value.int64 == 1602601886)',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'tutorial.PersonSearchService/Search') # grpc request
        assert grep_output(stdout, 'tutorial.Person') # grpc response

    def test_grpc_streaming_mode_reassembly(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC/HTTP2 streaming mode reassembly'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_stream_reassembly_sample.pcapng.gz'),
                '-d', 'tcp.port==50051,http2',
                '-d', 'tcp.port==44363,http2',
                '-2', # make http2.body.reassembled.in available
                '-Y', # Case1: In frame28, one http DATA contains 4 completed grpc messages (json data seq=1,2,3,4).
                      '(frame.number == 28 && grpc && json.value.number == 1 && json.value.number == 2'
                      ' && json.value.number == 3 && json.value.number == 4 && http2.body.reassembled.in == 45) ||'
                      # Case2: In frame28, last grpc message (the 5th) only has 4 bytes, which need one more byte
                      # to be a message head. a completed message is reassembled in frame45. (json data seq=5)
                      '(frame.number == 45 && grpc && http2.body.fragment == 28 && json.value.number == 5'
                      ' && http2.body.reassembled.in == 61) ||'
                      # Case3: In frame45, one http DATA frame contains two partial fragment, one is part of grpc
                      # message of previous http DATA (frame28), another is first part of grpc message of next http
                      # DATA (which will be reassembled in next http DATA frame61). (json data seq=6)
                      '(frame.number == 61 && grpc && http2.body.fragment == 45 && json.value.number == 6) ||'
                      # Case4: A big grpc message across frame100, frame113, frame126 and finally reassembled in frame139.
                      '(frame.number == 100 && grpc && http2.body.reassembled.in == 139) ||'
                      '(frame.number == 113 && !grpc && http2.body.reassembled.in == 139) ||'
                      '(frame.number == 126 && !grpc && http2.body.reassembled.in == 139) ||'
                      '(frame.number == 139 && grpc && json.value.number == 9) ||'
                      # Case5: An large grpc message of 200004 bytes.
                      '(frame.number == 164 && grpc && grpc.message_length == 200004)',
            ), encoding='utf-8', env=test_env)
        assert count_output(stdout, 'DATA') == 8

    def test_grpc_http2_fake_headers(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP2/gRPC fake headers (used when HTTP2 initial HEADERS frame is missing)'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_person_search_protobuf_with_image-missing_headers.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:http2_fake_headers: "{}","{}","{}","{}","{}","{}","{}"'.format(
                            '50051','3','IN',':path','/tutorial.PersonSearchService/Search','FALSE', 'TRUE'),
                '-o', 'uat:http2_fake_headers: "{}","{}","{}","{}","{}","{}","{}"'.format(
                            '50051','0','IN','content-type','application/grpc','FALSE','TRUE'),
                '-o', 'uat:http2_fake_headers: "{}","{}","{}","{}","{}","{}","{}"'.format(
                            '50051','0','OUT','content-type','application/grpc','FALSE','TRUE'),
                '-d', 'tcp.port==50051,http2',
                '-2',
                '-Y', 'protobuf.field.value.string == "Jason" || protobuf.field.value.string == "Lily"',
            ), encoding='utf-8', env=test_env)
        assert count_output(stdout, 'DATA') == 2


class TestDissectGrpcWeb:
    def test_grpc_web_unary_call_over_http1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web unary call over http1'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57226,http',
                '-2',
                '-Y', '(tcp.stream eq 0) && (pbf.greet.HelloRequest.name == "88888888"'
                        '|| pbf.greet.HelloRequest.name == "99999999"'
                        '|| pbf.greet.HelloReply.message == "Hello 99999999")',
            ), encoding='utf-8', env=test_env)
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 1

    def test_grpc_web_unary_call_over_http2(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web unary call over http2'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57228,http2',
                '-2',
                '-Y', '(tcp.stream eq 1) && (pbf.greet.HelloRequest.name == "88888888"'
                        '|| pbf.greet.HelloRequest.name == "99999999"'
                        '|| pbf.greet.HelloReply.message == "Hello 99999999")',
            ), encoding='utf-8', env=test_env)
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 1

    def test_grpc_web_reassembly_and_stream_over_http2(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web data reassembly and server stream over http2'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57228,http2',
                '-2',
                '-Y', '(tcp.stream eq 2) && ((pbf.greet.HelloRequest.name && grpc.message_length == 80004)'
                       '|| (pbf.greet.HelloReply.message && (grpc.message_length == 23 || grpc.message_length == 80012)))',
            ), encoding='utf-8', env=test_env)
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 4

    def test_grpc_web_text_unary_call_over_http1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web-Text unary call over http1'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57226,http',
                '-2',
                '-Y', '(tcp.stream eq 5) && (pbf.greet.HelloRequest.name == "88888888"'
                        '|| pbf.greet.HelloRequest.name == "99999999"'
                        '|| pbf.greet.HelloReply.message == "Hello 99999999")',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web-Text')
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 1

    def test_grpc_web_text_unary_call_over_http2(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web-Text unary call over http2'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57228,http2',
                '-2',
                '-Y', '(tcp.stream eq 6) && (pbf.greet.HelloRequest.name == "88888888"'
                        '|| pbf.greet.HelloRequest.name == "99999999"'
                        '|| pbf.greet.HelloReply.message == "Hello 99999999")',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web-Text')
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 1

    def test_grpc_web_text_reassembly_and_stream_over_http2(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web-Text data reassembly and server stream over http2'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57228,http2',
                '-2',
                '-Y', '(tcp.stream eq 8) && ((pbf.greet.HelloRequest.name && grpc.message_length == 80004)'
                       '|| (pbf.greet.HelloReply.message && (grpc.message_length == 23 || grpc.message_length == 80012)))',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web-Text')
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 4

    def test_grpc_web_text_reassembly_over_http1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web-Text data reassembly over http1'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57226,http',
                '-2',
                '-Y', '(tcp.stream eq 7) && (grpc.message_length == 80004 || grpc.message_length == 80010)',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web-Text')
        assert count_output(stdout, 'greet.HelloRequest') == 1
        assert count_output(stdout, 'greet.HelloReply') == 1

    def test_grpc_web_server_stream_over_http1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web data server stream over http1'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57226,http',
                '-2',
                '-Y', '(tcp.stream eq 9) && ((pbf.greet.HelloRequest.name && grpc.message_length == 10)'
                       '|| (pbf.greet.HelloReply.message && grpc.message_length == 18))',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web')
        assert count_output(stdout, 'greet.HelloRequest') == 1
        assert count_output(stdout, 'greet.HelloReply') == 9

    def test_grpc_web_reassembly_and_stream_over_http1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''gRPC-Web data reassembly and server stream over http1'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('grpc_web.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-d', 'tcp.port==57226,http',
                '-2',
                '-Y', '(tcp.stream eq 10) && ((pbf.greet.HelloRequest.name && grpc.message_length == 80004)'
                       '|| (pbf.greet.HelloReply.message && (grpc.message_length == 23 || grpc.message_length == 80012)))',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'GRPC-Web')
        assert count_output(stdout, 'greet.HelloRequest') == 2
        assert count_output(stdout, 'greet.HelloReply') == 6



class TestDissectHttp:
    def test_http_brotli_decompression(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP brotli decompression'''
        if not features.have_brotli:
            pytest.skip('Requires brotli.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http-brotli.pcapng'),
                '-Y', 'http.response.code==200',
                '-Tfields', '-etext',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'This is a test file for testing brotli decompression in Wireshark')

class TestDissectHttp2:
    def test_http2_data_reassembly(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP2 data reassembly'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        key_file = os.path.join(dirs.key_dir, 'http2-data-reassembly.keys')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http2-data-reassembly.pcap'),
                '-o', 'tls.keylog_file: {}'.format(key_file),
                '-d', 'tcp.port==8443,tls',
                '-Y', 'http2.data.data matches "PNG" && http2.data.data matches "END"',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'DATA')

    def test_http2_brotli_decompression(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP2 brotli decompression'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        if not features.have_brotli:
            pytest.skip('Requires brotli.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http2-brotli.pcapng'),
                '-Y', 'http2.data.data matches "This is a test file for testing brotli decompression in Wireshark"',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'DATA')

    def test_http2_zstd_decompression(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP/2 zstd decompression'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        if not features.have_zstd:
            pytest.skip('Requires zstd.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http2-zstd.pcapng'),
                '-Y', 'http2.data.data matches "Your browser supports decompressing Zstandard."',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'DATA')

    def test_http2_follow_0(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Follow HTTP/2 Stream ID 0 test'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        key_file = os.path.join(dirs.key_dir, 'http2-data-reassembly.keys')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http2-data-reassembly.pcap'),
                '-o', 'tls.keylog_file: {}'.format(key_file),
                '-z', 'follow,http2,hex,0,0'
            ), encoding='utf-8', env=test_env)
        # Stream ID 0 bytes
        assert grep_output(stdout, '00000000  00 00 12 04 00 00 00 00')
        # Stream ID 1 bytes, decrypted but compressed by HPACK
        assert not grep_output(stdout, '00000000  00 00 2c 01 05 00 00 00')
        # Stream ID 1 bytes, decrypted and uncompressed, human readable
        assert not grep_output(stdout, '00000000  3a 6d 65 74 68 6f 64 3a')

    def test_http2_follow_1(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Follow HTTP/2 Stream ID 1 test'''
        if not features.have_nghttp2:
            pytest.skip('Requires nghttp2.')
        key_file = os.path.join(dirs.key_dir, 'http2-data-reassembly.keys')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http2-data-reassembly.pcap'),
                '-o', 'tls.keylog_file: {}'.format(key_file),
                '-z', 'follow,http2,hex,0,1'
            ), encoding='utf-8', env=test_env)
        # Stream ID 0 bytes
        assert not grep_output(stdout, '00000000  00 00 12 04 00 00 00 00')
        # Stream ID 1 bytes, decrypted but compressed by HPACK
        assert not grep_output(stdout, '00000000  00 00 2c 01 05 00 00 00')
        # Stream ID 1 bytes, decrypted and uncompressed, human readable
        assert grep_output(stdout, '00000000  3a 6d 65 74 68 6f 64 3a')

class TestDissectHttp2:
    def test_http3_qpack_reassembly(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''HTTP/3 QPACK encoder stream reassembly'''
        if not features.have_nghttp3:
            pytest.skip('Requires nghttp3.')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('http3-qpack-reassembly-anon.pcapng'),
                '-Y', 'http3.frame_type == "HEADERS"',
                '-T', 'fields', '-e', 'http3.headers.method',
                '-e', 'http3.headers.authority',
                '-e', 'http3.headers.referer', '-e', 'http3.headers.user_agent',
                '-e', 'http3.qpack.encoder.icnt'
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'POST')
        assert grep_output(stdout, 'googlevideo.com')
        assert grep_output(stdout, 'https://www.youtube.com')
        assert grep_output(stdout, 'Mozilla/5.0')
        assert grep_output(stdout, '21') # Total number of QPACK insertions

class TestDissectProtobuf:
    def test_protobuf_udp_message_mapping(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf UDP Message Mapping and parsing google.protobuf.Timestamp features'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_udp_addressbook_with_image_ts.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:protobuf_udp_message_types: "8127","tutorial.AddressBook"',
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-Y', 'pbf.tutorial.Person.name == "Jason"'
                      ' && pbf.tutorial.Person.last_updated > "2020-10-15"'
                      ' && pbf.tutorial.Person.last_updated < "2020-10-19"',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'tutorial.AddressBook')

    def test_protobuf_message_type_leading_with_dot(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf Message type is leading with dot'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_test_leading_dot.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:protobuf_udp_message_types: "8123","a.b.msg"',
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-Y', 'pbf.a.b.a.b.c.param3 contains "in a.b.a.b.c" && pbf.a.b.c.param6 contains "in a.b.c"',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'PB[(]a.b.msg[)]')

    def test_protobuf_map_and_oneof_types(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf map and oneof types, and taking keyword as identification'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_test_map_and_oneof_types.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:protobuf_udp_message_types: "8124","test.map.MapMaster"',
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-Y', 'pbf.test.map.MapMaster.param3 == "I\'m param3 for oneof test."'  # test oneof type
                      ' && pbf.test.map.MapMaster.param4MapEntry.value == 1234'        # test map type
                      ' && pbf.test.map.Foo.param1 == 88 && pbf.test.map.MapMaster.param5MapEntry.key == 88'
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'PB[(]test.map.MapMaster[)]')

    def test_protobuf_default_value(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf feature adding missing fields with default values'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_test_default_value.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:protobuf_udp_message_types: "8128","wireshark.protobuf.test.TestDefaultValueMessage"',
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-o', 'protobuf.add_default_value: all',
                '-O', 'protobuf',
                '-Y', 'pbf.wireshark.protobuf.test.TestDefaultValueMessage.enumFooWithDefaultValue_Fouth == -4'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.boolWithDefaultValue_False == false'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.int32WithDefaultValue_0 == 0'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.doubleWithDefaultValue_Negative0point12345678 == -0.12345678'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.stringWithDefaultValue_SymbolPi contains "Pi."'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.bytesWithDefaultValue_1F2F890D0A00004B == 1f:2f:89:0d:0a:00:00:4b'
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.optional' # test taking keyword 'optional' as identification
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.message' # test taking keyword 'message' as identification
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.stringWithNoValue == ""' # test default value is empty for strings
                      ' && pbf.wireshark.protobuf.test.TestDefaultValueMessage.bytesWithNoValue == ""' # test default value is empty for bytes
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'floatWithDefaultValue_0point23: 0.23') # another default value will be displayed
        assert grep_output(stdout, 'missing required field \'missingRequiredField\'') # check the missing required field export warn

    def test_protobuf_field_subdissector(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test "protobuf_field" subdissector table'''
        if not features.have_lua:
            pytest.skip('Test requires Lua scripting support.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        lua_file = os.path.join(dirs.lua_dir, 'protobuf_test_field_subdissector_table.lua')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_udp_addressbook_with_image_ts.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'uat:protobuf_udp_message_types: "8127","tutorial.AddressBook"',
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-X', 'lua_script:{}'.format(lua_file),
                '-Y', 'pbf.tutorial.Person.name == "Jason" && pbf.tutorial.Person.last_updated && png',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'PB[(]tutorial.AddressBook[)]')

    def test_protobuf_called_by_custom_dissector(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf invoked by other dissector (passing type by pinfo.private)'''
        if not features.have_lua:
            pytest.skip('Test requires Lua scripting support.')
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        user_defined_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'user_defined_types').replace('\\', '/')
        lua_file = os.path.join(dirs.lua_dir, 'protobuf_test_called_by_custom_dissector.lua')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_tcp_addressbook.pcapng.gz'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(user_defined_types_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-X', 'lua_script:{}'.format(lua_file),
                '-d', 'tcp.port==18127,addrbook',
                '-Y', 'pbf.tutorial.Person.name == "Jason" && pbf.tutorial.Person.last_updated',
            ), encoding='utf-8', env=test_env)
        assert grep_output(stdout, 'tutorial.AddressBook')

    def test_protobuf_complex_syntax(self, cmd_tshark, features, dirs, capture_file, test_env):
        '''Test Protobuf parsing complex syntax .proto files'''
        well_know_types_dir = os.path.join(dirs.protobuf_lang_files_dir, 'well_know_types').replace('\\', '/')
        complex_proto_files_dir = os.path.join(dirs.protobuf_lang_files_dir, 'complex_proto_files').replace('\\', '/')
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('protobuf_udp_addressbook_with_image_ts.pcapng'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(well_know_types_dir, 'FALSE'),
                '-o', 'uat:protobuf_search_paths: "{}","{}"'.format(complex_proto_files_dir, 'TRUE'),
                '-o', 'protobuf.preload_protos: TRUE',
                '-o', 'protobuf.pbf_as_hf: TRUE',
                '-Y', 'pbf.wireshark.protobuf.test.complex.syntax.TestFileParsed.last_field_for_wireshark_test'
                      ' && pbf.protobuf_unittest.TestFileParsed.last_field_for_wireshark_test',
            ), encoding='utf-8', env=test_env)
        # the output must be empty and not contain something like:
        #   tshark: "pbf.xxx.TestFileParsed.last_field_for_wireshark_test" is neither a field nor a protocol name.
        # or
        #   tshark: Protobuf: Error(s)
        assert not grep_output(stdout, '.last_field_for_wireshark_test')
        assert not grep_output(stdout, 'Protobuf: Error')

class TestDissectTcp:
    @staticmethod
    def check_tcp_out_of_order(cmd_tshark, dirs, test_env, extraArgs=[]):
        capture_file = os.path.join(dirs.capture_dir, 'http-ooo.pcap')
        stdout = subprocess.check_output([cmd_tshark,
                '-r', capture_file,
                '-otcp.reassemble_out_of_order:TRUE',
                '-Y', 'http',
            ] + extraArgs, encoding='utf-8', env=test_env)
        assert count_output(stdout, 'HTTP') == 5
        assert grep_output(stdout, r'^\s*4\s.*PUT /1 HTTP/1.1')
        assert grep_output(stdout, r'^\s*7\s.*GET /2 HTTP/1.1')
        assert grep_output(stdout, r'^\s*10\s.*PUT /3 HTTP/1.1')
        assert grep_output(stdout, r'^\s*11\s.*PUT /4 HTTP/1.1')
        assert grep_output(stdout, r'^\s*15\s.*PUT /5 HTTP/1.1')

    def test_tcp_out_of_order_onepass(self, cmd_tshark, dirs, test_env):
        self.check_tcp_out_of_order(cmd_tshark, dirs, test_env)

    def test_tcp_out_of_order_twopass(self, cmd_tshark, dirs, test_env):
        self.check_tcp_out_of_order(cmd_tshark, dirs, test_env, extraArgs=['-2'])

    def test_tcp_out_of_order_data_after_syn(self, cmd_tshark, capture_file, test_env):
        '''Test when the first non-empty segment is OoO.'''
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('dns-ooo.pcap'),
                '-otcp.reassemble_out_of_order:TRUE',
                '-Y', 'dns', '-Tfields', '-edns.qry.name',
            ), encoding='utf-8', env=test_env)
        assert stdout.strip() == 'example.com'

    def test_tcp_out_of_order_first_gap(self, cmd_tshark, capture_file, test_env):
        '''
        Test reporting of "reassembled_in" in the OoO frame that contains the
        initial segment (Bug 15420). Additionally, test for proper reporting
        when the initial segment is retransmitted.
        For PDU H123 (where H is the HTTP Request header and 1, 2 and 3 are part
        of the body), the order is: (SYN) 2 H H 1 3 H.
        '''
        stdout = subprocess.check_output((cmd_tshark,
            '-r', capture_file('http-ooo2.pcap'),
            '-otcp.reassemble_out_of_order:TRUE',
            '-Tfields',
            '-eframe.number', '-etcp.reassembled_in', '-e_ws.col.info',
            '-2',
            ), encoding='utf-8', env=test_env)
        lines = stdout.split('\n')
        # 2 - start of OoO MSP
        assert '2\t6\t[TCP Previous segment not captured]' in lines[1]
        assert '[TCP segment of a reassembled PDU]' in lines[1] or '[TCP PDU reassembled in' in lines[1]

        # H - first time that the start of the MSP is delivered
        assert '3\t6\t[TCP Out-Of-Order]' in lines[2]
        assert '[TCP segment of a reassembled PDU]' in lines[2] or '[TCP PDU reassembled in' in lines[2]

        # H - first retransmission. Because this is before the reassembly
        # completes we can add it to the reassembly
        assert '4\t6\t[TCP Retransmission]' in lines[3]
        assert '[TCP segment of a reassembled PDU]' in lines[3] or '[TCP PDU reassembled in' in lines[3]

        # 1 - continue reassembly
        assert '5\t6\t[TCP Out-Of-Order]' in lines[4]
        assert '[TCP segment of a reassembled PDU]' in lines[4] or '[TCP PDU reassembled in' in lines[4]

        # 3 - finish reassembly
        assert '6\t\tPUT /0 HTTP/1.1' in lines[5]

        # H - second retransmission. This is after the reassembly completes
        # so we do not add it to the reassembly (but throw a ReassemblyError.)
        assert '7\t\t' in lines[6]
        assert '[TCP segment of a reassembled PDU]' not in lines[6] and '[TCP PDU reassembled in' not in lines[6]

    def test_tcp_reassembly_more_data_1(self, cmd_tshark, capture_file, test_env):
        '''
        Tests that reassembly also works when a new packet begins at the same
        sequence number as the initial segment. This models behavior with the
        ZeroWindowProbe: the initial segment contains a single byte. The second
        segment contains that byte, plus the remainder.
        '''
        stdout = subprocess.check_output((cmd_tshark,
            '-r', capture_file('retrans-tls.pcap'),
            '-Ytls', '-Tfields', '-eframe.number', '-etls.record.length',),
            encoding='utf-8', env=test_env)
        # First pass dissection actually accepted the first frame as TLS, but
        # subsequently requested reassembly.
        assert stdout == '1\t\n2\t16\n'

    def test_tcp_reassembly_more_data_2(self, cmd_tshark, capture_file, test_env):
        '''
        Like test_tcp_reassembly_more_data_1, but checks the second pass (-2).
        '''
        stdout = subprocess.check_output((cmd_tshark,
            '-r', capture_file('retrans-tls.pcap'),
            '-Ytls', '-Tfields', '-eframe.number', '-etls.record.length', '-2'),
            encoding='utf-8', env=test_env)
        assert stdout == '2\t16\n'

class TestDissectGit:
    def test_git_prot(self, cmd_tshark, capture_file, features, test_env):
        '''
        Check for Git protocol version 2, flush and delimiter packets.
        Ensure there are no malformed packets.
        '''
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('gitOverTCP.pcap'),
                '-Ygit', '-Tfields', '-egit.version', '-egit.packet_type',
                '-zexpert', '-e_ws.expert',
            ), encoding='utf-8', env=test_env)
        # `epan/dissectors/packet-git.c` parses the Git Protocol version
        # from ASCII '1' or '2' to integers 49 or 50 in grep output.
        # 0x0000 are flush packets.
        # 0x0001 are delimiter packets.
        # Pre-existing git Malformed Packets in this pcap were addressed
        # with the parsing of the delimiter packets. This test ensures
        # pcap gitOverTCP's delim packets are parsed and that there are no
        # malformed packets with "Expert Info/Errors" in the same pcap.
        # Additional test cases for other scenarios, i.e actually malformed
        # git packets, might be required.
        assert stdout == '50\t\t\n\t0\t\n\t\t\n\t1,0\t\n'

class TestDissectTls:
    @staticmethod
    def check_tls_handshake_reassembly(cmd_tshark, capture_file, test_env,
                                       extraArgs=[]):
        # Include -zexpert just to be sure that no exception has occurred. It
        # is not strictly necessary as the extension to be matched is the last
        # one in the handshake message.
        stdout = subprocess.check_output([cmd_tshark,
                               '-r', capture_file('tls-fragmented-handshakes.pcap.gz'),
                               '-zexpert',
                               '-Ytls.handshake.extension.data',
                               '-Tfields', '-etls.handshake.extension.data'] + extraArgs,
                               encoding='utf-8', env=test_env)
        stdout = stdout.replace(',', '\n')
        # Expected output are lines with 0001, 0002, ..., 03e8
        expected = ''.join('%04x\n' % i for i in range(1, 1001))
        assert stdout == expected

    @staticmethod
    def check_tls_reassembly_over_tcp_reassembly(cmd_tshark, capture_file, test_env,
                                                 extraArgs=[]):
        stdout = subprocess.check_output([cmd_tshark,
                               '-r', capture_file('tls-fragmented-over-tcp-segmented.pcapng.gz'),
                               '-zexpert,note',
                               '-Yhttp.host',
                               '-Tfields', '-ehttp.host'] + extraArgs,
                               encoding='utf-8', env=test_env)
        stdout = stdout.replace(',', '\n')
        assert stdout == 'reports.crashlytics.com\n'

    def test_tls_handshake_reassembly(self, cmd_tshark, capture_file, test_env):
        '''Verify that TCP and TLS handshake reassembly works.'''
        self.check_tls_handshake_reassembly(cmd_tshark, capture_file, test_env)

    def test_tls_handshake_reassembly_2(self, cmd_tshark, capture_file, test_env):
        '''Verify that TCP and TLS handshake reassembly works (second pass).'''
        self.check_tls_handshake_reassembly(
            cmd_tshark, capture_file, test_env, extraArgs=['-2'])

    def test_tls_reassembly_over_tcp_reassembly(self, cmd_tshark, capture_file, features, test_env):
        '''Verify that TLS reassembly over TCP reassembly works.'''
        if not features.have_gnutls:
            pytest.skip('Requires GnuTLS.')
        self.check_tls_reassembly_over_tcp_reassembly(cmd_tshark, capture_file, test_env)

    def test_tls_reassembly_over_tcp_reassembly_2(self, cmd_tshark, capture_file, features, test_env):
        '''Verify that TLS reassembly over TCP reassembly works (second pass).'''
        # pinfo->curr_layer_num can be different on the second pass than the
        # first pass, because the HTTP dissector isn't called for the first
        # TLS record on the second pass.
        if not features.have_gnutls:
            pytest.skip('Requires GnuTLS.')
        self.check_tls_reassembly_over_tcp_reassembly(cmd_tshark, capture_file,
            test_env, extraArgs=['-2'])

    @staticmethod
    def check_tls_out_of_order(cmd_tshark, capture_file, test_env, extraArgs=[]):
        stdout = subprocess.check_output([cmd_tshark,
                '-r', capture_file('challenge01_ooo_stream.pcapng.gz'),
                '-otcp.reassemble_out_of_order:TRUE',
                '-q',
                '-zhttp,stat,png or image-jfif',
            ] + extraArgs, encoding='utf-8', env=test_env)
        assert grep_output(stdout, r'200 OK\s*11')

    def test_tls_out_of_order(self, cmd_tshark, capture_file, features, test_env):
        '''Verify that TLS reassembly over TCP reassembly works.'''
        if not features.have_gnutls:
            pytest.skip('Requires GnuTLS.')
        self.check_tls_out_of_order(cmd_tshark, capture_file, test_env)

    def test_tls_out_of_order_second_pass(self, cmd_tshark, capture_file, features, test_env):
        '''Verify that TLS reassembly over TCP reassembly works (second pass).'''
        if not features.have_gnutls:
            pytest.skip('Requires GnuTLS.')
        self.check_tls_out_of_order(cmd_tshark, capture_file,
            test_env, extraArgs=['-2'])

class TestDissectQuic:
    @staticmethod
    def check_quic_tls_handshake_reassembly(cmd_tshark, capture_file, test_env,
                                       extraArgs=[]):
        # An assortment of QUIC carrying TLS handshakes that need to be
        # reassembled, including fragmented in one packet, fragmented in
        # multiple packets, fragmented in multiple out of order packets,
        # retried, retried with overlap from the original packets, and retried
        # with one of the original packets missing (but all data there.)
        # Include -zexpert just to be sure that nothing Warn or higher occurred.
        # Note level expert infos may be expected with the overlaps and
        # retransmissions.
        stdout = subprocess.check_output([cmd_tshark,
                               '-r', capture_file('quic-fragmented-handshakes.pcapng.gz'),
                               '-zexpert,warn',
                               '-Ytls.handshake.type',
                               '-o', 'gui.column.format:"Handshake Type","%Cus:tls.handshake.type:0:R"',
                               ] + extraArgs,
                               encoding='utf-8', env=test_env)
        assert count_output(stdout, 'Client Hello') == 18
        assert count_output(stdout, 'Server Hello') == 2
        assert count_output(stdout, 'Finished') == 2
        assert count_output(stdout, 'New Session Ticket,New Session Ticket') == 1
        assert count_output(stdout, 'Certificate') == 2
        assert not grep_output(stdout, 'Warns')
        assert not grep_output(stdout, 'Errors')

    def test_quic_tls_handshake_reassembly(self, cmd_tshark, capture_file, test_env):
        '''Verify that QUIC and TLS handshake reassembly works.'''
        self.check_quic_tls_handshake_reassembly(cmd_tshark, capture_file, test_env)

    def test_quic_tls_handshake_reassembly_2(self, cmd_tshark, capture_file, test_env):
        '''Verify that QUIC and TLS handshake reassembly works (second pass).'''
        self.check_quic_tls_handshake_reassembly(
            cmd_tshark, capture_file, test_env, extraArgs=['-2'])

    def test_quic_multiple_retry(self, cmd_tshark, capture_file, test_env):
        '''Verify that a second Retry is correctly ignored.'''
        stdout = subprocess.check_output([cmd_tshark,
                               '-r', capture_file('quic-double-retry.pcapng.gz'),
                               '-zexpert,warn',
                               ],
                               encoding='utf-8', env=test_env)
        assert not grep_output(stdout, 'Warns')
        assert not grep_output(stdout, 'Errors')

class TestDecompressSmb2:
    @staticmethod
    def extract_compressed_payload(cmd_tshark, capture_file, test_env, frame_num):
        stdout = subprocess.check_output((cmd_tshark,
                '-r', capture_file('smb311-lz77-lz77huff-lznt1.pcap.gz'),
                '-Tfields', '-edata.data',
                '-Y', 'frame.number == %d'%frame_num,
        ), encoding='utf-8', env=test_env)
        assert b'a'*4096 == bytes.fromhex(stdout.strip())

    def test_smb311_read_lz77(self, cmd_tshark, capture_file, test_env):
        self.extract_compressed_payload(cmd_tshark, capture_file, test_env, 1)

    def test_smb311_read_lz77huff(self, cmd_tshark, capture_file, test_env):
        self.extract_compressed_payload(cmd_tshark, capture_file, test_env, 2)

    def test_smb311_read_lznt1(self, cmd_tshark, capture_file, test_env):
        if sys.byteorder == 'big':
            pytest.skip('this test is supported on little endian only')
        self.extract_compressed_payload(cmd_tshark, capture_file, test_env, 3)

    def extract_chained_compressed_payload(self, cmd_tshark, capture_file, test_env, frame_num):
        stdout = subprocess.check_output((cmd_tshark,
            '-r', capture_file('smb311-chained-patternv1-lznt1.pcapng.gz'),
            '-Tfields', '-edata.data',
            '-Y', 'frame.number == %d'%frame_num,
        ), encoding='utf-8', env=test_env)
        assert b'\xaa'*256 == bytes.fromhex(stdout.strip())

    def test_smb311_chained_lznt1_patternv1(self, cmd_tshark, capture_file, test_env):
        if sys.byteorder == 'big':
            pytest.skip('this test is supported on little endian only')
        self.extract_chained_compressed_payload(cmd_tshark, capture_file, test_env, 1)

    def test_smb311_chained_none_patternv1(self, cmd_tshark, capture_file, test_env):
        self.extract_chained_compressed_payload(cmd_tshark, capture_file, test_env, 2)

class TestDissectCommunityId:
    @staticmethod
    def check_baseline(dirs, output, baseline):
        baseline_file = os.path.join(dirs.baseline_dir, baseline)
        with open(baseline_file) as f:
            baseline_data = f.read()

        assert output == baseline_data

    def test_communityid(self, cmd_tshark, features, dirs, capture_file, test_env):
        # Run tshark on our Community ID test pcap, enabling the
        # postdissector (it is disabled by default), and asking for
        # the Community ID value as field output. Verify that this
        # exits successfully:
        stdout = subprocess.check_output(
            (cmd_tshark,
             '--enable-protocol', 'communityid',
             '-r', capture_file('communityid.pcap.gz'),
             '-Tfields', '-ecommunityid.hash',
             ), encoding='utf-8', env=test_env)

        self.check_baseline(dirs, stdout, 'communityid.txt')

    def test_communityid_filter(self, cmd_tshark, features, dirs, capture_file, test_env):
        # Run tshark on our Community ID test pcap, enabling the
        # postdissector and filtering the result.
        stdout = subprocess.check_output(
            (cmd_tshark,
             '--enable-protocol', 'communityid',
             '-r', capture_file('communityid.pcap.gz'),
             '-Tfields', '-ecommunityid.hash',
             'communityid.hash=="1:d/FP5EW3wiY1vCndhwleRRKHowQ="'
             ), encoding='utf-8', env=test_env)

        self.check_baseline(dirs, stdout, 'communityid-filtered.txt')

class TestDecompressMongo:
    def test_decompress_zstd(self, cmd_tshark, features, capture_file, test_env):
        if not features.have_zstd:
            pytest.skip('Requires zstd.')
        stdout = subprocess.check_output((cmd_tshark,
                '-d', 'tcp.port==27017,mongo',
                '-r', capture_file('mongo-zstd.pcapng'),
                '-Tfields', '-emongo.element.name'
        ), encoding='utf-8', env=test_env)
        # Check the element names of the decompressed body.
        assert 'drop,lsid,id,$db' == stdout.strip()
