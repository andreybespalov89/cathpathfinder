import pytest
import datasetgradualstep_c as dsc

def test_algo_reverse_medium(): #Тест на реверс
    def custom_validator(chain):
        return True
    a_seq = "ABCDE"
    b_seq = "EDCBA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (distance == 4) and (steps == ['ABCDE', 'EDCBA']) and (operations[-1][0] == 'replace')

def test_algo_swap_start_medium(): #Тест на свап в конце
    def custom_validator(chain): 
        return True
    a_seq = "ABC"
    b_seq = "BAC"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(f'Последнее слово:{steps[-1]}')
    assert (steps == ['ABC', 'BAC']) and (operations[-1][0] == 'swap')

def test_algo_swap_end_medium(): #Тест на свап в конце
    def custom_validator(chain): 
        return True
    a_seq = "ABC"
    b_seq = "ACB"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(f'Последнее слово:{steps[-1]}')
    assert (steps == ['ABC', 'ACB']) and (operations[-1][0] == 'swap')

def test_algo_swap_medium(): #Тест на свап двойной
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "BACED"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (steps == ['ABCDE', 'BACDE', 'BACED']) and (operations[-1][0] == 'swap')

def test_algo_swap_partial_reverse(): #Самое дешевое это три свапа 
    def custom_validator(chain): 
        return True
    a_seq = "ABCDEF"
    b_seq = "BADCFE"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    assert distance == 3
def test_algo_swap_insert():#Свап плюс добавление
    def custom_validator(chain): 
        return True
    a_seq = "BAXX"
    b_seq = "ABXXC"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['', 'BA', 'BAX'])

def test_algo_swap_insert():#Свап плюс добавление
    def custom_validator(chain): 
        return True
    a_seq = "BAXX"
    b_seq = "ABXXC"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['BAXX', 'ABXX', 'ABXXC'])
def test_algo_swap_replace():#Свап плюс удаление
    def custom_validator(chain): 
        return True
    a_seq = "ABC"
    b_seq = "BAX"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['ABC', 'BAC', 'BAX'])

def test_algo_swap_replace(): #Еще тест на свап
    def custom_validator(chain): 
        return True
    a_seq = "XABY"
    b_seq = "XXBAY"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (steps == ['XABY', 'XXABY', 'XXBAY'])



    


