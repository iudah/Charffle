package io.github.iudah.charffle

import android.Manifest
import android.content.Context
import android.content.Intent
import android.content.res.AssetManager
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.provider.Settings
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
import io.github.iudah.charffle.ui.theme.AppTheme
import java.io.File
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

fun Context.showToast(text: String) {
  Toast.makeText(this, text, Toast.LENGTH_LONG).show()
}

val LOGTAG = "CharffleActivity"

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
        window.statusBarColor = Color.Transparent.toArgb()
        window.navigationBarColor = Color.Transparent.toArgb()
        WindowCompat.getInsetsController(window, window.decorView).isAppearanceLightNavigationBars =
            !isSystemInDarkTheme()

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
      val dir = "/data/data/com.zeroone.charffle/files/"
      val errmsg = Array<String>(1) { "" }
      val a_words_file = File(/*filesDir.absolutePath*/ dir, "words/a_words.txt")
      if (!a_words_file.exists()) {
        if (!extractWordsAsset(assets, /*filesDir.absolutePath*/ dir, errmsg))
            (this as Context).showToast(errmsg[0])

        (this as Context).showToast(a_words_file.path.toString() + " failed")
      } else {
        if (registerPrivatePath(/*filesDir.absolutePath*/ dir, errmsg))
            (this as Context).showToast("private path registered: ${errmsg[0]}")
      }
      if (a_words_file.exists())
          (this as Context).showToast(a_words_file.path.toString() + " success")
    } catch (e: Exception) {
      (this as Context).showToast(e.toString())
    }

    requestPermissions(
        arrayOf(
            Manifest.permission.WRITE_EXTERNAL_STORAGE,
            Manifest.permission.READ_EXTERNAL_STORAGE,
        ),
        6,
    )
    // Check for external storage
    if (Build.VERSION.SDK_INT >= 30 && !Environment.isExternalStorageManager())
        startActivity(Intent(Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION))
  }

  companion object {

    @JvmStatic external fun shuffle(chars: String): Array<String>

    @JvmStatic
    external fun extractWordsAsset(
        assetMan: AssetManager,
        fpath: String,
        errm: Array<String>? = null,
    ): Boolean

    @JvmStatic external fun registerPrivatePath(fpath: String, errm: Array<String>? = null): Boolean

    init {
      try {
        System.loadLibrary("charffle_c")
      } catch (e: UnsatisfiedLinkError) {
        (this as Context).showToast("Library load failed: ${e.message}")
      }
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
    InputCard(
        chars,
        { chars = it },
        {
          val msg = Array<String>(1) { "" }
          scope.launch(Dispatchers.IO) {
            try {
              list = CharffleActivity.shuffle(chars.toString())

              withContext(Dispatchers.Main) { ctx.showToast(msg[0]) }
            } catch (e: Exception) {
              withContext(Dispatchers.Main) { ctx.showToast(e.toString()) }
            }
          }
        },
    )

    ElevatedCard(
        elevation = CardDefaults.cardElevation(defaultElevation = 5.dp),
        colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
        modifier = Modifier.fillMaxWidth().fillMaxHeight(),
    ) {
      if (!list.isEmpty()) ShuffledListGrid(list)
    }
  }
}

@Composable
fun InputCard(chars: String, onCharsChange: (String) -> Unit, onShuffle: () -> Unit) {
  ElevatedCard(
      elevation = CardDefaults.cardElevation(defaultElevation = 5.dp),
      colors = CardDefaults.cardColors(containerColor = MaterialTheme.colorScheme.surfaceVariant),
      modifier = Modifier.fillMaxWidth(),
  ) {
    Column(
        modifier = Modifier.fillMaxWidth().size(130.dp).padding(8.dp),
        verticalArrangement = Arrangement.SpaceBetween,
    ) {
      OutlinedTextField(
          value = chars,
          onValueChange = onCharsChange,
          singleLine = true,
          label = { Text("Characters") },
          modifier = Modifier.fillMaxWidth(),
      )

      Row(horizontalArrangement = Arrangement.End, modifier = Modifier.fillMaxWidth()) {
        Button(onClick = onShuffle) { Text("Shuffle") }
      }
    }
  }
}

@Composable
fun ShuffledListGrid(list: Array<String>) {
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
