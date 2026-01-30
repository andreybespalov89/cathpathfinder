from AlgoDynamic import AlgoSeqDynamic
class AlgoSeqDynamicWithValidation(AlgoSeqDynamic):
    def __init__(self, validator_function=None):
        super().__init__()
        self.validator = validator_function or self._default_validator
    
    def _default_validator(self, chain):
        return True
    
    def _is_valid_chain(self, chain):
        return self.validator(chain)
    
    def _try_find_valid_path(self, start_chain, operations):
        current = list(start_chain)
        results = [''.join(current)]
        i = 0
        
        for op in operations:
            if op[0] == 'match':
                i += 1
            elif op[0] == 'replace':
                old_char, new_char = op[1], op[2]
                current[i] = new_char
                chain = ''.join(current)
                if not self._is_valid_chain(chain):
                    return None
                results.append(chain)
                i += 1
            elif op[0] == 'insert':
                char = op[1]
                current.insert(i, char)
                chain = ''.join(current)
                if not self._is_valid_chain(chain):
                    return None
                results.append(chain)
                i += 1
            elif op[0] == 'delete':
                char = op[1]
                current.pop(i)
                chain = ''.join(current)
                if not self._is_valid_chain(chain):
                    return None
                results.append(chain)
        
        return results
    
    def _find_all_paths_up_to_k(self, i, j, current_path, max_ops):
        if i == 0 and j == 0:
            return [current_path]
        
        if len(current_path) >= max_ops:
            return []
        
        paths = []
        
        # Все возможные операции
        if i > 0 and j > 0 and self.a_seq[i-1] == self.b_seq[j-1]:
            paths.extend(self._find_all_paths_up_to_k(i-1, j-1, 
                       [('match', self.a_seq[i-1])] + current_path, max_ops))
        
        if i > 0 and j > 0:
            paths.extend(self._find_all_paths_up_to_k(i-1, j-1,
                       [('replace', self.a_seq[i-1], self.b_seq[j-1])] + current_path, max_ops))
        
        if j > 0:
            paths.extend(self._find_all_paths_up_to_k(i, j-1,
                       [('insert', self.b_seq[j-1])] + current_path, max_ops))
        
        if i > 0:
            paths.extend(self._find_all_paths_up_to_k(i-1, j,
                       [('delete', self.a_seq[i-1])] + current_path, max_ops))
        
        return paths
    
    def _steps_(self):
        for max_ops in range(self.min_dist, self.min_dist + 5): 
            all_operations = self._find_all_paths_up_to_k(self.m, self.n, [], max_ops)
            
            if not all_operations:
                continue
            
            unique_ops = []
            seen = set()
            for ops in all_operations:
                key = str(ops)
                if key not in seen:
                    seen.add(key)
                    unique_ops.append(ops)
            
            for operations in unique_ops:
                steps = self._try_find_valid_path(self.a_seq, operations)
                if steps is not None:
                    return steps
        
        return None
    
    def run(self, a_seq, b_seq):
        self.a_seq = a_seq
        self.b_seq = b_seq
        self.dp = None
    
        self._init_matrix_()
        self._calculate_matrix_()
        
        self.min_dist = self.dp[self.m][self.n]  
        
        steps = self._steps_()
        
        if steps is None:
            return None, None, None
        
        operations = []
        for i in range(len(steps) - 1):
            prev, curr = steps[i], steps[i+1]
            if len(curr) > len(prev):
                # insert
                for j in range(len(curr)):
                    if j >= len(prev) or (j < len(prev) and curr[j] != prev[j]):
                        operations.append(('insert', curr[j]))
                        break
            elif len(curr) < len(prev):
                for j in range(len(prev)):
                    if j >= len(curr) or (j < len(curr) and curr[j] != prev[j]):
                        operations.append(('delete', prev[j]))
                        break
            else:
                diff_found = False
                for j in range(len(curr)):
                    if curr[j] != prev[j]:
                        operations.append(('replace', prev[j], curr[j]))
                        diff_found = True
                        break
                if not diff_found:
                    operations.append(('match', curr[0] if curr else ''))
        
        return steps, operations, len(operations)


