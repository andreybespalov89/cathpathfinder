
import pytest
import datasetgradualstep_c as dsc

def test_algo_replace(): #Тест на замену одной буквы на другую
    def custom_validator(chain):
        return True
    a_seq = "A"
    b_seq = "B"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    assert (distance == 1) and (steps == ['A', 'B']) and (operations[-1][0] == 'replace')

def test_algo_delete(): #Тест на удаление буквы
    def custom_validator(chain):
        return True
    a_seq = "AA"
    b_seq = "A"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    assert (steps == ['AA', 'A']) and (operations[-1][0] == 'delete')

def test_algo_insert(): #Тест на добавление буквы
    def custom_validator(chain):
        return True
    a_seq = "A"
    b_seq = "AA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    assert (steps == ['A', 'AA']) and (operations[-1][0] == 'insert')


def test_algo_few_insert(): 
    def custom_validator(chain):
        return True
    a_seq = "A"
    b_seq = "ABCDE"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (steps == ['A', 'AB', 'ABC', 'ABCD', 'ABCDE']) and (operations[-1][0] == 'insert')

def test_algo_few_delete(): #Тест на удаление букв
    def custom_validator(chain):
        return True
    a_seq = "ABCDE"
    b_seq = "A"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (steps == ['ABCDE', 'ABCD', 'ABC', 'AB', 'A']) and (operations[-1][0] == 'delete')

def test_algo_center_insert(): #Добавление в центр
    def custom_validator(chain): 
        return True
    a_seq = "ADE"
    b_seq = "ABCDE"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    assert (steps == ['ADE', 'ABDE', 'ABCDE']) and (operations[-1][0] == 'insert')


def test_algo_reverse(): #Тест на свап 
    def custom_validator(chain): 
        return True
    a_seq = "AB"
    b_seq = "BA"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(f'Последнее слово:{steps[-1]}')
    assert (steps == ['AB', 'BA']) and (operations[-1][0] == 'reverse')

def test_single_char(): #Единичная буква
    steps, ops, dist = dsc.algo_seq_dynamic_run("a", "a")
    assert dist == 0
    assert ops == [("match", "a")]

def test_algo_reverse_base():
    def custom_validator(chain): 
        return True
    a_seq = "EDADE"
    b_seq = "EDADE"
    steps, operations, distance =  dsc.algo_seq_dynamic_with_validation_run(a_seq, b_seq, validator=custom_validator)
    print(steps)
    print(operations)
    assert (distance == 0)
