def merge_files(skip_bytes, file1_path, file2_path, output_file_path):
    with open(file1_path, 'rb') as file1, open(file2_path, 'rb') as file2, open(output_file_path, 'wb') as output_file:

        file1.seek(skip_bytes)
        

        output_file.write(file2.read()[:skip_bytes])
        
        output_file.write(file1.read())


if __name__ == '__main__':
    merge_files(0xa73f0, './libc.so.rewritten', 'bin', 'libc.so.randomized')
