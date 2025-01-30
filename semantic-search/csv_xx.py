import pandas as pd

# create a sample DataFrame
df = pd.DataFrame({
    'column1': [1, 2, 3],
    'column2': [[4,5,6], [4,5,6], [4,5,6]]
})

# save the DataFrame to disk in CSV format
df.to_pickle('dataframe.pickle')

# read the saved CSV file back into memory as DataFrame
new_df = pd.read_pickle('dataframe.pickle')

# inspect the data types of the new DataFrame
print(df.dtypes)
print(new_df.dtypes)
