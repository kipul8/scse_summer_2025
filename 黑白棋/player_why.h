if(player -> mat[0][0] == 'O')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[0][i] += 20;
            player -> mat[i][0] += 20;
            player -> mat[i][i] += 20;
        }
    }
    if(player -> mat[0][0] == 'o')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[0][i] -= 20;
            player -> mat[i][0] -= 20;
            player -> mat[i][i] -= 20;
        }
    }
    if(player -> mat[0][player -> col_cnt - 1] == 'O')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[0][player -> col_cnt - i - 1] += 20;
            player -> mat[i][player -> col_cnt - 1] += 20;
            player -> mat[i][player -> col_cnt - i - 1] += 20;
        }
    }
    if(player -> mat[0][player -> col_cnt - 1] == 'o')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[0][player -> col_cnt - i - 1] -= 20;
            player -> mat[i][player -> col_cnt - 1] -= 20;
            player -> mat[i][player -> col_cnt - i - 1] -= 20;
        }
    }
    if(player -> mat[player -> row_cnt - 1][0] == 'O')
    {
        for(int i = 0; i < player -> col_cnt; i++)
        {
            player -> mat[player -> row_cnt - 1][i] += 20;
            player -> mat[player -> row_cnt - i - 1][0] += 20;
            player -> mat[player -> row_cnt - i - 1][i] += 20;
        }
    }
    if(player -> mat[player -> row_cnt - 1][0] == 'o')
    {
        for(int i = 0; i < player -> col_cnt; i++)
        {
            player -> mat[player -> row_cnt - 1][i] -= 20;
            player -> mat[player -> row_cnt - i - 1][0] -= 20;
            player -> mat[player -> row_cnt - i - 1][i] -= 20;
        }
    }
    if(player -> mat[player -> row_cnt - 1][player -> col_cnt - 1] == 'O')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[player -> row_cnt - 1][player -> col_cnt - i - 1] += 20;
            player -> mat[player -> row_cnt - i - 1][player -> col_cnt - 1] += 20;
            player -> mat[player -> row_cnt - i - 1][player -> col_cnt - i - 1] += 20;
        }
    }
    if(player -> mat[player -> row_cnt - 1][player -> col_cnt - 1] == 'o')
    {
        for(int i = 0; i < player -> row_cnt; i++)
        {
            player -> mat[player -> row_cnt - 1][player -> col_cnt - i - 1] -= 20;
            player -> mat[player -> row_cnt - i - 1][player -> col_cnt - 1] -= 20;
            player -> mat[player -> row_cnt - i - 1][player -> col_cnt - i - 1] -= 20;
        }
    }
