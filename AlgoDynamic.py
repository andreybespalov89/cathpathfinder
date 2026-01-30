class AlgoSeqDynamic():
    def __init__(self):
        self.a_seq = None
        self.b_seq = None 
        self.dp = None 
        self.min_dist = None
        self.m = None
        self.n = None
        
        
    def _init_matrix_(self):
        self.m, self.n = len(self.a_seq), len(self.b_seq)
        dp = [[0] * (self.n + 1) for _ in range(self.m + 1)]

        for i in range(self.m + 1):
                dp[i][0] = i
        for j in range(self.n + 1):
                dp[0][j] = j
        self.dp = dp

        
    def _calculate_matrix_(self):
        for i in range(1, self.m + 1):
            for j in range(1, self.n + 1):
                if self.a_seq[i-1] == self.b_seq[j-1]:
                    self.dp[i][j] = self.dp[i-1][j-1]
                else:
                    self.dp[i][j] = min(self.dp[i-1][j] + 1, self.dp[i][j-1] + 1, self.dp[i-1][j-1] + 1)
    def _reverse_(self):
        operations = []
        i, j = self.m, self.n
            
        while i > 0 or j > 0:
            if i > 0 and j > 0 and self.a_seq[i-1] == self.b_seq[j-1]:
                operations.append(('match', self.a_seq[i-1]))
                i -= 1
                j -= 1
            else:
                if i > 0 and j > 0 and self.dp[i][j] == self.dp[i-1][j-1] + 1:
                    operations.append(('replace', self.a_seq[i-1], self.b_seq[j-1]))
                    i -= 1
                    j -= 1
                elif j > 0 and self.dp[i][j] == self.dp[i][j-1] + 1:
                    operations.append(('insert', self.b_seq[j-1]))
                    j -= 1
                elif i > 0 and self.dp[i][j] == self.dp[i-1][j] + 1:
                    operations.append(('delete', self.a_seq[i-1]))
                    i -= 1
            
        operations.reverse()
        return operations
        
    def _steps_(self):
        current = list(self.a_seq)  
        operations = self._reverse_()  
        
        results = []
        results.append(''.join(current))
        
        
        i = 0  
        step = 1
        
        for op in operations:
            if op[0] == 'match':
                i += 1
            elif op[0] == 'replace':
                old_char, new_char = op[1], op[2]
                current[i] = new_char
                results.append(''.join(current))
                i += 1
            elif op[0] == 'insert':
                char = op[1]
                current.insert(i, char)
                results.append(''.join(current))
                i += 1
            elif op[0] == 'delete':
                char = op[1]
                current.pop(i)
                results.append(''.join(current))
        
        return results

        

        
    def run(self, a_seq, b_seq):
        self.a_seq = a_seq
        self.b_seq = b_seq
        self.dp = None
    
        self._init_matrix_()
        self._calculate_matrix_()
        
        min_distance = self.dp[self.m][self.n]  # Исправлено: self.m, self.n
        operations = self._reverse_()
        
        steps = self._steps_()
        return steps, operations, min_distance