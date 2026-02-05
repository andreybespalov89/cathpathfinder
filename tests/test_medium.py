import pytest
import datasetgradualstep_c as dsc



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

def test_algo_swap_partial_reverse_medium(): #Самое дешевое это три свапа 
    def custom_validator(chain): 
        return True
    a_seq = "ABCDEF"
    b_seq = "BADCFE"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    assert distance == 3
def test_algo_swap_insert_medium():#Свап плюс добавление
    def custom_validator(chain): 
        return True
    a_seq = "BAXX"
    b_seq = "ABXXC"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['', 'BA', 'BAX'])

def test_algo_swap_insert_medium():#Свап плюс добавление
    def custom_validator(chain): 
        return True
    a_seq = "BAXX"
    b_seq = "ABXXC"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['BAXX', 'ABXX', 'ABXXC'])
def test_algo_swap_replace_medium():#Свап плюс удаление
    def custom_validator(chain): 
        return True
    a_seq = "ABC"
    b_seq = "BAX"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['ABC', 'BAC', 'BAX'])

def test_algo_swap_replace_medium():
    def custom_validator(chain): 
        return True
    a_seq = "XABY"
    b_seq = "XXBAY"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (steps == ['XABY', 'XXABY', 'XXBAY'])

def test_algo_swap_medium_3_swap():
    def custom_validator(chain): 
        return True
    a_seq = "XACCCXAXA"
    b_seq = "AXCCCAXAX"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (steps == ['XACCCXAXA', 'AXCCCXAXA', 'AXCCCAXXA', 'AXCCCAXAX'])



def test_algo_reverse_medium():
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "EDCBAA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (steps == ['ABCDE', 'EDCBA', 'EDCBAA']) and (operations[0][0] == 'reverse')

    

def test_algo_reverse_medium_insert_two():
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "XEDCBAX"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 3) and (steps == ['ABCDE', 'EDCBA', 'XEDCBA', 'XEDCBAX'])


def test_algo_reverse_medium_remove():
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "EDCB"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['ABCDE', 'EDCBA','EDCB'])



def test_algo_reverse_medium_and_swap():
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "ECDBA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 2) and (steps == ['ABCDE', 'EDCBA', 'ECDBA'])

def test_algo_reverse_medium_and_other_operations():
    def custom_validator(chain): 
        return True
    a_seq = "ABCDE"
    b_seq = "AECDBAA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert  steps == ['ABCDE', 'EDCBA', 'AEDCBA', 'AECDBA', 'AECDBAA']