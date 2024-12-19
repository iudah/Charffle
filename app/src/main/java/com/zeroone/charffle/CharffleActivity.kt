package com.zeroone.charffle

import android.content.res.AssetManager
import android.os.Bundle
import android.widget.Toast
import androidx.activity.ComponentActivity
import androidx.activity.compose.setContent
import androidx.compose.foundation.isSystemInDarkTheme
import androidx.compose.foundation.layout.Arrangement
import androidx.compose.foundation.layout.Box
import androidx.compose.foundation.layout.Column
import androidx.compose.foundation.layout.Row
import androidx.compose.foundation.layout.fillMaxHeight
import androidx.compose.foundation.layout.fillMaxWidth
import androidx.compose.foundation.layout.padding
import androidx.compose.foundation.layout.size
import androidx.compose.foundation.layout.statusBarsPadding
import androidx.compose.foundation.lazy.grid.GridCells
import androidx.compose.foundation.lazy.grid.LazyVerticalGrid
import androidx.compose.material3.Button
import androidx.compose.material3.CardDefaults
import androidx.compose.material3.ElevatedCard
import androidx.compose.material3.MaterialTheme
import androidx.compose.material3.OutlinedTextField
import androidx.compose.material3.Surface
import androidx.compose.material3.Text
import androidx.compose.runtime.Composable
import androidx.compose.runtime.getValue
import androidx.compose.runtime.mutableStateOf
import androidx.compose.runtime.rememberCoroutineScope
import androidx.compose.runtime.saveable.rememberSaveable
import androidx.compose.runtime.setValue
import androidx.compose.ui.Modifier
import androidx.compose.ui.graphics.Color
import androidx.compose.ui.graphics.toArgb
import androidx.compose.ui.platform.LocalContext
import androidx.compose.ui.text.font.FontWeight
import androidx.compose.ui.text.style.TextAlign
import androidx.compose.ui.unit.dp
import androidx.compose.ui.unit.sp
import androidx.core.view.WindowCompat
import com.zeroone.charffle.ui.theme.AppTheme
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class CharffleActivity : ComponentActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        // upgrade gradle and libs to use this
        // enableEdgeToEdge()
        WindowCompat.setDecorFitsSystemWindows(window, false)

        setContent {
            AppTheme {
                window.setNavigationBarContrastEnforced(false)
                window.setStatusBarContrastEnforced(false)
                window.statusBarColor = MaterialTheme.colorScheme.primary.toArgb()
                window.navigationBarColor = Color.Transparent.toArgb()
                WindowCompat.getInsetsController(window, window.decorView)
                    .isAppearanceLightNavigationBars = !isSystemInDarkTheme()

                Surface(tonalElevation = 5.dp) {
                    Box(modifier = Modifier.statusBarsPadding()) {
                        /*Spacer(
                            Modifier.windowInsetsTopHeight(
                                WindowInsets.systemBars
                            )
                        )*/
                        CharffleScreen()
                    }
                }
            }
            try {
                actionBar?.hide()
            } catch (e: Exception) {
                Toast.makeText(this, e.toString(), Toast.LENGTH_LONG).show()
            }
        }
        try {
            val a_words_file = File(getFilesDir().getAbsolutePath(), "words/a_words.txt")
            if (!a_words_file.exists()) {
                val errmsg = Array<String>(1) { "" }
                if (
                    !extractWordsAsset(
                        getResources().getAssets(),
                        getFilesDir().getAbsolutePath().toString(),
                        errmsg,
                    )
                )
                    Toast.makeText(this, errmsg[0], Toast.LENGTH_LONG).show()

                Toast.makeText(this, a_words_file.path.toString() + " failed", Toast.LENGTH_LONG)
                    .show()
            } else {
                if (registerPrivatePath(getFilesDir().getAbsolutePath()))
                    Toast.makeText(this, "private path registered", Toast.LENGTH_LONG).show()
            }
            if (a_words_file.exists())
                Toast.makeText(this, a_words_file.path.toString() + " success", Toast.LENGTH_LONG)
                    .show()
        } catch (e: Exception) {
            Toast.makeText(this, e.toString(), Toast.LENGTH_LONG).show()
        }
    }

    companion object {

        @JvmStatic external fun shuffle(chars: String): Array<String>

        @JvmStatic
        external fun extractWordsAsset(
            assetMan: AssetManager,
            fpath: String,
            errm: Array<String>? = null,
        ): Boolean

        @JvmStatic
        external fun registerPrivatePath(fpath: String, errm: Array<String>? = null): Boolean

        init {

            System.loadLibrary("charffle_c")
        }
    }
}

@Composable
fun CharffleScreen() {
    var chars by rememberSaveable { mutableStateOf("") }
    val padding = 6.dp
    var list by rememberSaveable { mutableStateOf(emptyArray<String>()) }
    val ctx = LocalContext.current
    val scope = rememberCoroutineScope()

    Column(
        modifier = Modifier.padding(padding).fillMaxHeight().fillMaxWidth(),
        verticalArrangement = Arrangement.spacedBy(space = 24.dp),
    ) {
        ElevatedCard(
            elevation = CardDefaults.cardElevation(defaultElevation = 5.dp),
            colors =
                CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            modifier = Modifier.fillMaxWidth(),
        ) {
            Column(
                modifier = Modifier.fillMaxWidth().size(130.dp).padding(padding),
                verticalArrangement = Arrangement.SpaceBetween,
            ) {
                OutlinedTextField(
                    value = chars,
                    onValueChange = { chars = it },
                    singleLine = true,
                    label = { Text("Characters") },
                    modifier = Modifier.fillMaxWidth(),
                )

                Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
                    Button(
                        onClick = {
                            val msg = Array<String>(1) { "" }
                            scope.launch(Dispatchers.IO) {                                    list = CharffleActivity.shuffle(chars.toString())

                                withContext(Dispatchers.Main) {

                                    Toast.makeText(ctx, msg[0], Toast.LENGTH_LONG).show()
                                }
                            }
                        }
                    ) {
                        Text("Shuffle")
                    }
                }
            }
        }

        ElevatedCard(
            elevation = CardDefaults.cardElevation(defaultElevation = 5.dp),
            colors =
                CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
            modifier = Modifier.fillMaxWidth().fillMaxHeight(),
        ) {
            if (list.size > 0)
                LazyVerticalGrid(
                    columns = GridCells.Adaptive(128.dp),
                    modifier = Modifier.fillMaxWidth().fillMaxHeight(),
                ) {
                    items(list.size) { index ->
                        ElevatedCard(modifier = Modifier.padding(6.dp).fillMaxWidth()) {
                            Text(
                                text = list[index],
                                fontWeight = FontWeight.Bold,
                                fontSize = 30.sp,
                                textAlign = TextAlign.Center,
                                modifier = Modifier.padding(10.dp),
                            )
                        }
                    }
                }
        }
    }
}
