static const int display_tabla[16] = 
{

}


// convertir a minusculas
value = to_lower(value);

if(value >= '1' && value <= '9')
{
    display_tabla[value];
}
else if(value >= 'a' && value <= 'h')
{
    display_tabla[value - 'a'];
}
else
{
    // error valor inválido
}