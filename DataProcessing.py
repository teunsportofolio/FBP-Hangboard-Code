import pandas as pd
import numpy as np

def is_standard_row(row):
    """
    Returns True if the row is a standard row: two columns, and the second column is a float.
    """
    if len(row) != 2:
        return False
    try:
        float(row[1])
        return True
    except (ValueError, TypeError):
        return False

def split_on_special_rows(csv_filepath, export_filepath, min_average):
    # Read the CSV as a list of lists, not as a DataFrame
    with open(csv_filepath, "r", encoding="utf-8") as f:
        lines = f.readlines()
    # Strip and split lines
    rows = [list(map(str.strip, line.strip().split(','))) for line in lines if line.strip()]

    output_data = []
    current_chunk = []

    for row in rows:
        if not is_standard_row(row):
            if current_chunk:
                # Remove empty strings from chunk before appending
                cleaned_chunk = [x for x in current_chunk if x != ""]
                if cleaned_chunk:
                    output_data.append(cleaned_chunk)
                current_chunk = []
        else:
            # Only append the second column (the float value), drop the timestamp
            if row[1] != "":
                current_chunk.append(row[1])

    # Save any remaining chunk
    if current_chunk:
        cleaned_chunk = [x for x in current_chunk if x != ""]
        if cleaned_chunk:
            output_data.append(cleaned_chunk)

    # Pad rows to the same length for DataFrame export
    maxlen = max(len(chunk) for chunk in output_data)
    padded_data = [chunk + [np.nan] * (maxlen - len(chunk)) for chunk in output_data]
    df = pd.DataFrame(padded_data)

    # Switch rows and columns (transpose)
    df_transposed = df.transpose()

    # Convert all values to float where possible (NaNs remain)
    df_numeric = df_transposed.apply(pd.to_numeric, errors='coerce')

    # Calculate mean for each column, and filter columns by minimum average
    means = df_numeric.mean(skipna=True)
    columns_to_keep = means[means >= min_average].index
    df_filtered = df_transposed[columns_to_keep]

    # Export to CSV
    df_filtered.to_csv(export_filepath, index=False, header=False)

if __name__ == "__main__":
    # Insert your file paths here
    input_csv = "input.csv"         # <--- Insert your input CSV file path here
    output_csv = "exported.csv"     # <--- Insert your desired export file path here
    MIN_AVERAGE = 5.0               # <--- Set your minimum average value here
    split_on_special_rows(input_csv, output_csv, MIN_AVERAGE)
    print(f"Exported split, transposed, and filtered data to {output_csv}")