import pytest
import datasetgradualstep_c.datasetgradualstep_c as dsc

def test_algo_reverse(): #Тест на реверс
    def custom_validator(chain):
        return True
    a_seq = "ABCDE"
    b_seq = "EDCBA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (distance == 4) and (steps == ['ABCDE', 'EDCBA']) and (operations[-1][0] == 'replace')
